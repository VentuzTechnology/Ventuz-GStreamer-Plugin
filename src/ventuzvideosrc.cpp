
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
        "alignment = au, "
        "profile = main, "
        "framerate = " GST_VIDEO_FPS_RANGE "; "

        "video/x-h265, "
        "stream-format = byte-stream, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "alignment = au, "
        "framerate = " GST_VIDEO_FPS_RANGE "; "
    )
);

static void ventuz_video_src_on_video(void* opaque, const uint8_t* data, size_t size, int64_t timecode, bool isIDR)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(opaque);

    // wait for first IDR frame
    if (!self->gotIDR)
    {
        if (isIDR)
            self->gotIDR = true;
        else
            return;
    }

    GstElement* elem = GST_ELEMENT(opaque);
    GstBaseSrc* base = GST_BASE_SRC(opaque);

    GstBuffer* buffer = gst_buffer_new_memdup(data, size);

    // timestamps
    auto& header = self->outputHeader;
    GstClockTimeDiff dur = GST_SECOND * header.videoFrameRateDen / header.videoFrameRateNum;
    GST_BUFFER_DURATION(buffer) = dur;

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(self));
    if (clock)
    {
        GstClockTime baseTime = gst_element_get_base_time(GST_ELEMENT(self));
        GST_BUFFER_TIMESTAMP(buffer) = gst_clock_get_time(clock) + StreamOutPipe::OutputManager::Instance.GetTimeDiff(timecode) - baseTime;
        gst_object_unref(clock);
    }

    g_mutex_lock(&self->lock);

    if (g_queue_get_length(self->frames) < VentuzVideoSrc::MAX_Q)
    {
        g_queue_push_tail(self->frames, buffer);
        g_cond_signal(&self->cond);
    }
    else
        gst_buffer_unref(buffer);

    g_mutex_unlock(&self->lock);
}

static void ventuz_video_src_init(VentuzVideoSrc* self)
{
    self->outputNumber = 0;
    self->flushing = false;
    self->gotIDR = false;
    self->frames = g_queue_new();

    gst_base_src_set_live(GST_BASE_SRC(self), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_TIME);

    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);

    GST_OBJECT_FLAG_SET(self, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
    GST_OBJECT_FLAG_SET(self, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
}

static void ventuz_video_src_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    switch (property_id)
    {
    case PROP_OUTPUT_NUMBER:
        g_value_set_int(value, self->outputNumber);
        break;
    }
}

static void ventuz_video_src_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    switch (property_id)
    {
    case PROP_OUTPUT_NUMBER:
        self->outputNumber = g_value_get_int(value);
        break;
    }

}

static void ventuz_video_src_finalize(GObject* object)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    g_mutex_clear(&self->lock);
    g_cond_clear(&self->cond);
}

static GstClock* ventuz_video_src_provide_clock(GstElement* elem)
{
    return StreamOutPipe::OutputManager::Instance.GetClock();
}


static void ventuz_video_src_output_start(void* opaque, const StreamOutPipe::PipeHeader& header)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(opaque);
    GstBaseSrc* bsrc = GST_BASE_SRC(opaque);

    self->outputHeader = header;

    GstCaps* caps;

    switch (header.videoCodecFourCC)
    {
    case 'h264': caps = gst_caps_new_simple("video/x-h264", NULL); break;
    case 'hevc': caps = gst_caps_new_simple("video/x-h265", NULL); break;
    default:
        // TODO: some error
        return;
    }

    GstStructure* video_structure = gst_caps_get_structure(caps, 0);

    gst_structure_set(video_structure, "stream-format", G_TYPE_STRING, "byte-stream", NULL);
    gst_structure_set(video_structure, "alignment", G_TYPE_STRING, "au", NULL);
    gst_structure_set(video_structure, "profile", G_TYPE_STRING, "main", NULL);

    gst_structure_set(video_structure, "width", G_TYPE_INT, header.videoWidth, NULL);
    gst_structure_set(video_structure, "height", G_TYPE_INT, header.videoHeight, NULL);
    gst_structure_set(video_structure, "framerate", GST_TYPE_FRACTION, header.videoFrameRateNum, header.videoFrameRateDen, NULL);

    gst_base_src_set_caps(bsrc, caps);

    gst_caps_unref(caps);
}

static void ventuz_video_src_output_stop(void* opaque)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(opaque);
    GstBaseSrc* bsrc = GST_BASE_SRC(opaque);
}

static gboolean ventuz_video_src_start(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    self->gotIDR = false;
    self->outputHandle = StreamOutPipe::OutputManager::Instance.Acquire(self->outputNumber, {
        .opaque = bsrc,
        .onStart = ventuz_video_src_output_start,
        .onStop = ventuz_video_src_output_stop,
        .onVideo = ventuz_video_src_on_video,
        });

    gst_base_src_start_complete(bsrc, GST_FLOW_OK);

    return TRUE;
}

static gboolean ventuz_video_src_stop(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    StreamOutPipe::OutputManager::Instance.Release(self->outputNumber, &self->outputHandle);

    g_mutex_unlock(&self->lock);
    return TRUE;
}

static gboolean ventuz_video_src_query(GstBaseSrc* bsrc, GstQuery* query)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_LATENCY: 
        if (self->outputHeader.videoFrameRateDen)
        {
            auto& header = self->outputHeader;

            guint64 min = gst_util_uint64_scale_ceil(GST_SECOND, header.videoFrameRateDen, header.videoFrameRateNum);
            guint64 max = min * VentuzVideoSrc::MAX_Q;

            gst_query_set_latency(query, TRUE, min, max);
            return TRUE;
        return FALSE;
        }      
    
    default:
        return GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
    }
}

static gboolean ventuz_video_src_unlock(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);
    self->flushing = TRUE;
    g_cond_signal(&self->cond);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean ventuz_video_src_unlock_stop(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    self->flushing = FALSE;

    while (!g_queue_is_empty(self->frames))
    {
        GstBuffer* buffer = (GstBuffer*)g_queue_pop_head(self->frames);
        gst_buffer_unref(buffer);
    }

    g_mutex_unlock(&self->lock);

    return TRUE;
}

static GstFlowReturn ventuz_video_src_create(GstPushSrc* psrc, GstBuffer** buffer)
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

        if (!g_queue_is_empty(self->frames))
        {
            *buffer = (GstBuffer*)g_queue_pop_head(self->frames);
            flow_ret = GST_FLOW_OK;
            break;
        }

        g_cond_wait(&self->cond, &self->lock);
    }

    g_mutex_unlock(&self->lock);
    return flow_ret;
}

static void ventuz_video_src_class_init(VentuzVideoSrcClass* klass)
{

    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
    GstPushSrcClass* pushsrc_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* basesrc_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Ventuz Stream Out video source",
        "Source/Video",
        "Receives the video stream from a Ventuz Stream Out output",
        "Ventuz Technology <tammo.hinrichs@ventuz.com>");

    gobject_class->set_property = ventuz_video_src_set_property;
    gobject_class->get_property = ventuz_video_src_get_property;
    gobject_class->finalize = ventuz_video_src_finalize;

    element_class->provide_clock = GST_DEBUG_FUNCPTR(ventuz_video_src_provide_clock);

    basesrc_class->negotiate = NULL;
    basesrc_class->query = GST_DEBUG_FUNCPTR(ventuz_video_src_query);
    basesrc_class->unlock = GST_DEBUG_FUNCPTR(ventuz_video_src_unlock);
    basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(ventuz_video_src_unlock_stop);
    basesrc_class->start = GST_DEBUG_FUNCPTR(ventuz_video_src_start);
    basesrc_class->stop = GST_DEBUG_FUNCPTR(ventuz_video_src_stop);

    pushsrc_class->create = GST_DEBUG_FUNCPTR(ventuz_video_src_create);

    g_object_class_install_property(gobject_class, PROP_OUTPUT_NUMBER,
        g_param_spec_int("output-number", "Output number",
            "Ventuz output number to use", 0, 7, 0,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    gst_element_class_add_static_pad_template(element_class, &src_template);
}

GST_ELEMENT_REGISTER_DEFINE(ventuzvideosrc, "ventuzvideosrc", GST_RANK_NONE, GST_TYPE_VENTUZ_VIDEO_SRC);
