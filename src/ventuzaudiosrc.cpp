/* Ventuz GStreamer plugin
 * Copyright (C) 2024 Ventuz Technology <tammo.hinrichs@ventuz.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "ventuzaudiosrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_ventuz_audio_src_debug);
#define GST_CAT_DEFAULT gst_ventuz_audio_src_debug

#define parent_class ventuz_audio_src_parent_class
G_DEFINE_TYPE(VentuzAudioSrc, ventuz_audio_src, GST_TYPE_PUSH_SRC);

enum
{
    PROP_0,
    PROP_OUTPUT_NUMBER,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw, format=(string)S16LE, channels=(int)2, rate=(int)48000, layout=(string)interleaved, channel-mask=(bitmask)0x3")
);


static void ventuz_audio_src_output_start(void* opaque, const StreamOutPipe::PipeHeader& header)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(opaque);
    GstBaseSrc* bsrc = GST_BASE_SRC(opaque);

    self->outputHeader = header;

    GstCaps* caps;

    switch (header.audioCodecFourCC)
    {
    case 'pc16': caps = gst_caps_new_simple("audio/x-raw", NULL); break;
    default:
        // TODO: some error
        return;
    }

    GstStructure* audio_structure = gst_caps_get_structure(caps, 0);

    gst_structure_set(audio_structure, "format", G_TYPE_STRING, "S16LE", NULL);
    gst_structure_set(audio_structure, "layout", G_TYPE_STRING, "interleaved", NULL);
    gst_structure_set(audio_structure, "channels", G_TYPE_INT, header.audioChannels, NULL);
    gst_structure_set(audio_structure, "rate", G_TYPE_INT, header.audioRate, NULL);
    gst_structure_set(audio_structure, "channel-mask", GST_TYPE_BITMASK, 3, NULL);

    gst_base_src_set_caps(bsrc, caps);

    gst_caps_unref(caps);
}

static void ventuz_audio_src_output_stop(void* opaque)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(opaque);
    GstBaseSrc* bsrc = GST_BASE_SRC(opaque);
}

static void ventuz_audio_src_on_audio(void* opaque, const uint8_t* data, size_t size, int64_t timecode)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(opaque);

    GstElement* elem = GST_ELEMENT(opaque);
    GstBaseSrc* base = GST_BASE_SRC(opaque);

    GstBuffer* buffer = gst_buffer_new_memdup(data, size);

    // timestamps
    auto& header = self->outputHeader;
    GstClockTimeDiff dur = size * GST_SECOND / (4 * header.audioRate);
    GST_BUFFER_DURATION(buffer) = dur;

    GstClock* clock = gst_element_get_clock(GST_ELEMENT(self));
    if (clock)
    {
        GstClockTime baseTime = gst_element_get_base_time(GST_ELEMENT(self));
        GST_BUFFER_TIMESTAMP(buffer) = gst_clock_get_time(clock) + StreamOutPipe::OutputManager::Instance.GetTimeDiff(timecode) - baseTime;
        gst_object_unref(clock);
    }

    g_mutex_lock(&self->lock);

    if (g_queue_get_length(self->packets) < VentuzAudioSrc::MAX_Q)
    {
        g_queue_push_tail(self->packets, buffer);
        g_cond_signal(&self->cond);
    }
    else
        gst_buffer_unref(buffer);

    g_mutex_unlock(&self->lock);
}

static void ventuz_audio_src_init(VentuzAudioSrc* self)
{
    self->outputNumber = 0;
    self->flushing = false;
    self->packets = g_queue_new();

    gst_base_src_set_live(GST_BASE_SRC(self), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(self), GST_FORMAT_TIME);

    g_mutex_init(&self->lock);
    g_cond_init(&self->cond);

    GST_OBJECT_FLAG_SET(self, GST_ELEMENT_FLAG_PROVIDE_CLOCK);
    GST_OBJECT_FLAG_SET(self, GST_ELEMENT_FLAG_REQUIRE_CLOCK);
}

static void ventuz_audio_src_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(object);

    switch (property_id)
    {
    case PROP_OUTPUT_NUMBER:
        g_value_set_int(value, self->outputNumber);
        break;
    }
}

static void ventuz_audio_src_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(object);

    switch (property_id)
    {
    case PROP_OUTPUT_NUMBER:
        self->outputNumber = g_value_get_int(value);
        break;
    }

}

static void ventuz_audio_src_finalize(GObject* object)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(object);

    g_mutex_lock(&self->lock);

    while (!g_queue_is_empty(self->packets))
    {
        GstBuffer* buffer = (GstBuffer*)g_queue_pop_head(self->packets);
        gst_buffer_unref(buffer);
    }

    g_mutex_unlock(&self->lock);
    g_mutex_clear(&self->lock);
    g_cond_clear(&self->cond);
}

static GstClock* ventuz_audio_src_provide_clock(GstElement* elem)
{
    GstClock *clock = StreamOutPipe::OutputManager::Instance.GetClock();
    gst_object_ref_sink(clock);
    return clock;
}

static gboolean ventuz_audio_src_query(GstBaseSrc* bsrc, GstQuery* query)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(bsrc);

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_LATENCY:
        if (self->outputHeader.videoFrameRateDen)
        {
            auto& header = self->outputHeader;

            guint64 min = gst_util_uint64_scale_ceil(GST_SECOND, header.videoFrameRateDen, header.videoFrameRateNum);
            guint64 max = min * VentuzAudioSrc::MAX_Q;

            gst_query_set_latency(query, TRUE, min, max);
            return TRUE;
            return FALSE;
        }

    default:
        return GST_BASE_SRC_CLASS(parent_class)->query(bsrc, query);
    }
}

static gboolean ventuz_audio_src_unlock(GstBaseSrc* bsrc)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);
    self->flushing = TRUE;
    g_cond_signal(&self->cond);
    g_mutex_unlock(&self->lock);

    return TRUE;
}

static gboolean ventuz_audio_src_unlock_stop(GstBaseSrc* bsrc)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    self->flushing = FALSE;

    while (!g_queue_is_empty(self->packets))
    {
        GstBuffer* buffer = (GstBuffer*)g_queue_pop_head(self->packets);
        gst_buffer_unref(buffer);
    }

    g_mutex_unlock(&self->lock);

    return TRUE;
}


static gboolean ventuz_audio_src_start(GstBaseSrc* bsrc)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(bsrc);

    self->outputHandle = StreamOutPipe::OutputManager::Instance.Acquire(self->outputNumber, {
        .opaque = bsrc,
        .onStart = ventuz_audio_src_output_start,
        .onStop = ventuz_audio_src_output_stop,
        .onAudio = ventuz_audio_src_on_audio,
        });

    gst_base_src_start_complete(bsrc, GST_FLOW_OK);

    return TRUE;
}

static gboolean ventuz_audio_src_stop(GstBaseSrc* bsrc)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(bsrc);

    g_mutex_lock(&self->lock);

    StreamOutPipe::OutputManager::Instance.Release(self->outputNumber, &self->outputHandle);

    g_mutex_unlock(&self->lock);
    return TRUE;
}

static GstFlowReturn ventuz_audio_src_create(GstPushSrc* psrc, GstBuffer** buffer)
{
    VentuzAudioSrc* self = VENTUZ_AUDIO_SRC_CAST(psrc);
    GstFlowReturn flow_ret = GST_FLOW_ERROR;

    g_mutex_lock(&self->lock);
    for (;;)
    {
        if (self->flushing)
        {
            flow_ret = GST_FLOW_FLUSHING;
            break;
        }

        if (!g_queue_is_empty(self->packets))
        {
            *buffer = (GstBuffer*)g_queue_pop_head(self->packets);
            flow_ret = GST_FLOW_OK;
            break;
        }

        g_cond_wait(&self->cond, &self->lock);
    }

    g_mutex_unlock(&self->lock);
    return flow_ret;
}


static void ventuz_audio_src_class_init(VentuzAudioSrcClass* klass)
{

    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
    GstPushSrcClass* pushsrc_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass* basesrc_class = GST_BASE_SRC_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Ventuz Stream Out audio source",
        "Source/Audio",
        "Receives the audio stream from a Ventuz Stream Out output",
        "Ventuz Technology <tammo.hinrichs@ventuz.com>");

    gobject_class->set_property = ventuz_audio_src_set_property;
    gobject_class->get_property = ventuz_audio_src_get_property;
    gobject_class->finalize = ventuz_audio_src_finalize;

    element_class->provide_clock = GST_DEBUG_FUNCPTR(ventuz_audio_src_provide_clock);

    basesrc_class->negotiate = NULL;
    basesrc_class->query = GST_DEBUG_FUNCPTR(ventuz_audio_src_query);
    basesrc_class->unlock = GST_DEBUG_FUNCPTR(ventuz_audio_src_unlock);
    basesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(ventuz_audio_src_unlock_stop);
    basesrc_class->start = GST_DEBUG_FUNCPTR(ventuz_audio_src_start);
    basesrc_class->stop = GST_DEBUG_FUNCPTR(ventuz_audio_src_stop);

    pushsrc_class->create = GST_DEBUG_FUNCPTR(ventuz_audio_src_create);

    g_object_class_install_property(gobject_class, PROP_OUTPUT_NUMBER,
        g_param_spec_int("output-number", "Output number",
            "Ventuz output number to use", 0, 7, 0,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    gst_element_class_add_static_pad_template(element_class, &src_template);
}



GST_ELEMENT_REGISTER_DEFINE(ventuzaudiosrc, "ventuzaudiosrc", GST_RANK_NONE, GST_TYPE_VENTUZ_AUDIO_SRC);