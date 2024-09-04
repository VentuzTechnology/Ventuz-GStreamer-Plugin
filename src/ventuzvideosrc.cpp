
#include "ventuzvideosrc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC(gst_ventuz_video_src_debug);
#define GST_CAT_DEFAULT gst_ventuz_video_src_debug

#define parent_class ventuz_video_src_parent_class
G_DEFINE_TYPE(VentuzVideoSrc, ventuz_video_src, GST_TYPE_PUSH_SRC);

enum
{
    PROP_0,
    PROP_OUTPUT_NUMBER,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-h264, "
        "stream-format = byte-stream, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE "; "

        "video/x-hevc, "
        "stream-format = byte-stream, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE "; "
    )
);

static void OnVideo(void* opaque, const uint8_t* data, size_t size, int64_t timecode, bool isIDR)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(opaque);
    GstElement* elem = GST_ELEMENT(opaque);
    GstBaseSrc* base = GST_BASE_SRC(opaque);

    if (self->nextFrame)
        gst_buffer_unref(self->nextFrame);
    self->nextFrame = gst_buffer_new_memdup(data, size);

    // timestamps
    auto& header = self->client.GetHeader();
    gint64 ts = base->segment.start + GST_SECOND * self->frameCount * header.videoFrameRateDen / header.videoFrameRateNum;
    gint64 dur = GST_SECOND * header.videoFrameRateDen / header.videoFrameRateNum;
    GST_BUFFER_TIMESTAMP(self->nextFrame) = ts;
    GST_BUFFER_DURATION(self->nextFrame) = dur;
    self->frameCount++;

}

static void ventuz_video_src_init(VentuzVideoSrc* self)
{
    self->outputNumber = 0;
    self->flushing = false;
    self->client.SetOnVideo(OnVideo, self);
    self->frameCount = 0;

    gst_base_src_set_live(GST_BASE_SRC(self), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_TIME);

    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);

}

static void get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    switch (property_id) {
    case PROP_OUTPUT_NUMBER:
        g_value_set_int(value, self->outputNumber);
        break;
    }
}

static void set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    switch (property_id) {
    case PROP_OUTPUT_NUMBER:
        self->outputNumber = g_value_get_int(value);
        break;
    }

}

static void finalize(GObject* object)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    g_mutex_clear(&self->lock);
    g_cond_clear(&self->cond);
}

static GstStateChangeReturn change_state(GstElement* element, GstStateChange transition)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        ret = GST_STATE_CHANGE_NO_PREROLL;
        break;
    default:
        break;
    }

    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;
    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED: {

        GST_DEBUG_OBJECT(self, "Stopping streams");
        ret = GST_STATE_CHANGE_NO_PREROLL;

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING: {
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
        break;

    default:
        break;
    }

    return ret;
}

static GstCaps* fixate(GstBaseSrc* src, GstCaps* caps)
{
    return caps;
}

static gboolean start(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);
    
    g_mutex_lock(&self->lock);
    while (!self->client.Open(self->outputNumber))
    {
        guint64 wait_time = g_get_monotonic_time() + G_TIME_SPAN_MILLISECOND * 100;
        g_cond_wait_until(&self->cond, &self->lock, wait_time);
        if (self->flushing)
        {
            g_mutex_unlock(&self->lock);
            return FALSE;
        }
    }

    g_mutex_unlock(&self->lock);

    // make caps from header
    auto& header = self->client.GetHeader();

    GstCaps* caps;

    switch (header.videoCodecFourCC)
    {
    case 'h264': caps = gst_caps_new_simple("video/x-h264", NULL); break;
    case 'hevc': caps = gst_caps_new_simple("video/x-hevc", NULL); break;
    default:  
        g_mutex_unlock(&self->lock);
        return FALSE;
    }

    GstStructure* video_structure = gst_caps_get_structure(caps, 0);

    gst_structure_set(video_structure, "stream-format", G_TYPE_STRING, "byte-stream", NULL);
    gst_structure_set(video_structure, "alignment", G_TYPE_STRING, "au", NULL);
    gst_structure_set(video_structure, "profile", G_TYPE_STRING, "main", NULL);
     
    gst_structure_set(video_structure, "width", G_TYPE_INT, header.videoWidth, NULL);
    gst_structure_set(video_structure, "height", G_TYPE_INT, header.videoHeight, NULL);
    //gst_structure_set(video_structure, "framerate", GST_TYPE_FRACTION, header.videoFrameRateNum, header.videoFrameRateDen, NULL);

    gst_base_src_set_caps(bsrc, caps);

    gst_caps_unref(caps);

    gst_base_src_start_complete(bsrc, GST_FLOW_OK);

    return TRUE;
}

static gboolean stop(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    g_mutex_unlock(&self->lock);
    return TRUE;
}

static gboolean query(GstBaseSrc* bsrc, GstQuery* query)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
        if (self->client.IsOpen()) {

            auto &header = self->client.GetHeader();

            guint64 min = gst_util_uint64_scale_ceil(GST_SECOND, header.videoFrameRateDen, header.videoFrameRateNum);
            guint64 max =  min;

            gst_query_set_latency(query, TRUE, min, max);
            return TRUE;
        }
        else
            return FALSE;

        return FALSE;
    }
    default:
        return GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
    }
}

static gboolean unlock(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);
    self->flushing = TRUE;
    g_cond_signal(&self->cond);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean unlock_stop(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    self->flushing = FALSE;

    if (self->nextFrame)
    {
        gst_buffer_unref(self->nextFrame);
        self->nextFrame = nullptr;
    }

    g_mutex_unlock(&self->lock);


    return TRUE;
}

static GstFlowReturn create(GstPushSrc* psrc, GstBuffer** buffer)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(psrc);
    GstFlowReturn flow_ret = GST_FLOW_ERROR;

    g_mutex_lock(&self->lock);
    for (;;)
    {
        if (self->flushing)
        {
            flow_ret = GST_FLOW_FLUSHING;
            break;
        }

        bool ok = self->client.Poll();
        if (!ok)
        {
            flow_ret = GST_FLOW_ERROR;
            break;
        }

        if (self->nextFrame)
        {                        
            *buffer = self->nextFrame;
            self->nextFrame = nullptr;
            flow_ret = GST_FLOW_OK;
            break;
        }

        guint64 wait_time = g_get_monotonic_time() + G_TIME_SPAN_MILLISECOND;
        g_cond_wait_until(&self->cond, &self->lock, wait_time);
    }

    g_mutex_unlock(&self->lock);
    return flow_ret;
}

static void ventuz_video_src_class_init(VentuzVideoSrcClass* klass)
{

    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
    GstBaseSrcClass* basesrc_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass* pushsrc_class = GST_PUSH_SRC_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Ventuz Stream Out video source",
        "Video/Source/Software",
        "Receives the video stream from a Ventuz Stream Out output",
        "Ventuz Technology <your.name@your.isp>");


    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;
    gobject_class->finalize = finalize;

    element_class->change_state = GST_DEBUG_FUNCPTR(change_state);

    basesrc_class->query = GST_DEBUG_FUNCPTR(query);
    basesrc_class->negotiate = NULL;
    basesrc_class->unlock = GST_DEBUG_FUNCPTR(unlock);
    basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(unlock_stop);
    basesrc_class->start = GST_DEBUG_FUNCPTR(start);
    basesrc_class->stop = GST_DEBUG_FUNCPTR(stop);
    basesrc_class->fixate = GST_DEBUG_FUNCPTR(fixate);


    pushsrc_class->create = GST_DEBUG_FUNCPTR(create);

    g_object_class_install_property(gobject_class, PROP_OUTPUT_NUMBER,
        g_param_spec_int("output-number", "Output number",
            "Ventuz output number to use", 0, 7, 0,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    gst_element_class_add_static_pad_template(element_class, &src_template);
}

GST_ELEMENT_REGISTER_DEFINE(ventuzvideosrc, "ventuzvideosrc", GST_RANK_NONE,
    GST_TYPE_VENTUZ_VIDEO_SRC);
