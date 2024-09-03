
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

static void get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);
}

static void set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    switch (property_id) {
    case PROP_OUTPUT_NUMBER:
        g_value_set_int((GValue*)value, self->outputNumber);
        break;
    }
}

static void finalize(GObject* object)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(object);

    // TODO: clean up
}

static GstStateChangeReturn change_state(GstElement* element, GstStateChange transition)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
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

static gboolean query(GstBaseSrc* bsrc, GstQuery* query)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
        /*
        if (self->input) {
            GstClockTime min, max;
            const GstDecklinkMode* mode;

            g_mutex_lock(&self->lock);
            mode = ventuz_get_mode(self->caps_mode);
            g_mutex_unlock(&self->lock);

            min = gst_util_uint64_scale_ceil(GST_SECOND, mode->fps_d, mode->fps_n);
            max = self->buffer_size * min;

            gst_query_set_latency(query, TRUE, min, max);
            ret = TRUE;
        }
        else {
            ret = FALSE;
        }
        */

        return FALSE;
    }
    default:
        return GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
    }
}

static GstCaps* get_caps(GstBaseSrc* bsrc, GstCaps* filter)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);
    GstCaps* caps;

    /*
    if (self->mode != GST_DECKLINK_MODE_AUTO) {
        const GstDecklinkMode* gst_mode = ventuz_get_mode(self->mode);
        BMDDynamicRange dynamic_range = device_dynamic_range(self);
        caps =
            ventuz_mode_get_caps(self->mode,
                display_mode_flags(self, gst_mode, FALSE), self->caps_format,
                dynamic_range, TRUE);
    }
    else if (self->caps_mode != GST_DECKLINK_MODE_AUTO) {
        const GstDecklinkMode* gst_mode = ventuz_get_mode(self->caps_mode);
        BMDDynamicRange dynamic_range = device_dynamic_range(self);
        caps =
            ventuz_mode_get_caps(self->caps_mode,
                display_mode_flags(self, gst_mode, FALSE), self->caps_format,
                dynamic_range, TRUE);
    }
    */
    if (false)
    {

    }
    else 
    {
        caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(bsrc));
    }

    if (filter) {
        GstCaps* tmp =
            gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(caps);
        caps = tmp;
    }

    return caps;
}



static gboolean unlock(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    /*
    g_mutex_lock(&self->lock);
    self->flushing = TRUE;
    g_cond_signal(&self->cond);
    g_mutex_unlock(&self->lock);
    */
    return TRUE;
}

static gboolean unlock_stop(GstBaseSrc* bsrc)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(bsrc);

    /*
    g_mutex_lock(&self->lock);
    self->flushing = FALSE;
    while (gst_vec_deque_get_length(self->current_frames) > 0) {
        CaptureFrame* tmp =
            (CaptureFrame*)gst_vec_deque_pop_head_struct(self->current_frames);
        capture_frame_clear(tmp);
    }
    g_mutex_unlock(&self->lock);
    */

    return TRUE;
}

static GstFlowReturn create(GstPushSrc* psrc, GstBuffer** buffer)
{
    VentuzVideoSrc* self = VENTUZ_VIDEO_SRC_CAST(psrc);
    GstFlowReturn flow_ret = GST_FLOW_ERROR;

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
    basesrc_class->get_caps = GST_DEBUG_FUNCPTR(get_caps);
    basesrc_class->unlock = GST_DEBUG_FUNCPTR(unlock);
    basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(unlock_stop);

    pushsrc_class->create = GST_DEBUG_FUNCPTR(create);

    g_object_class_install_property(gobject_class, PROP_OUTPUT_NUMBER,
        g_param_spec_int("output-number", "Output number",
            "Ventuz output number to use", 0, 7, 0,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    gst_element_class_add_static_pad_template(element_class, &src_template);
}

static void ventuz_video_src_init(VentuzVideoSrc* self)
{
    self->outputNumber = 0;

    gst_base_src_set_live(GST_BASE_SRC(self), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_TIME);

    gst_pad_use_fixed_caps(GST_BASE_SRC_PAD(self));

    /*
    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);

    self->current_frames =
        gst_vec_deque_new_for_struct(sizeof(CaptureFrame),
            DEFAULT_BUFFER_SIZE);
            */
}

GST_ELEMENT_REGISTER_DEFINE(ventuzvideosrc, "ventuzvideosrc", GST_RANK_NONE,
    GST_TYPE_VENTUZ_VIDEO_SRC);
