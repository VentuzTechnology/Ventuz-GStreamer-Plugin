#pragma once

#pragma warning (push)
#pragma warning (disable: 4244)

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/audio/audio.h>

#pragma warning (pop)

#include "streamoutpipe.h"

G_BEGIN_DECLS

#define GST_TYPE_VENTUZ_AUDIO_SRC (ventuz_audio_src_get_type())
#define VENTUZ_AUDIO_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VENTUZ_AUDIO_SRC, VentuzAudioSrc))
#define VENTUZ_AUDIO_SRC_CAST(obj) ((VentuzAudioSrc*)obj)
#define VENTUZ_AUDIO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VENTUZ_AUDIO_SRC, VentuzAudioSrcClass))
#define GST_IS_VENTUZ_AUDIO_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VENTUZ_AUDIO_SRC))
#define GST_IS_VENTUZ_AUDIO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VENTUZ_AUDIO_SRC))

struct VentuzAudioSrc
{
    GstPushSrc parent;

    int outputNumber;
    bool flushing;

    void* outputHandle;
    StreamOutPipe::PipeHeader outputHeader;

    GCond cond;
    GMutex lock;
    GQueue* packets;

    static const int MAX_Q = 5;
};

struct VentuzAudioSrcClass
{
    GstPushSrcClass parent_class;
};

GType ventuz_audio_src_get_type(void);

GST_ELEMENT_REGISTER_DECLARE(ventuzaudiosrc);

G_END_DECLS