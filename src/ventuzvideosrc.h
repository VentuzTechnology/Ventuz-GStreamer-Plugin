#pragma once

#pragma warning (push)
#pragma warning (disable: 4244)

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>

#pragma warning (pop)

#include "streamoutpipe.h"

G_BEGIN_DECLS

#define GST_TYPE_VENTUZ_VIDEO_SRC (ventuz_video_src_get_type())
#define VENTUZ_VIDEO_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VENTUZ_VIDEO_SRC, VentuzVideoSrc))
#define VENTUZ_VIDEO_SRC_CAST(obj) ((VentuzVideoSrc*)obj)
#define VENTUZ_VIDEO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VENTUZ_VIDEO_SRC, VentuzVideoSrcClass))
#define GST_IS_VENTUZ_VIDEO_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VENTUZ_VIDEO_SRC))
#define GST_IS_VENTUZ_VIDEO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VENTUZ_VIDEO_SRC))

struct VentuzVideoSrc
{
    GstPushSrc parent;

    int outputNumber;
    bool flushing;

    void* outputHandle;
    StreamOutPipe::PipeHeader outputHeader;
    bool gotIDR;

    GCond cond;
    GMutex lock;
    GQueue* frames;
    gint64 frameCount;

    static const int MAX_Q = 5;
};

struct VentuzVideoSrcClass
{
    GstPushSrcClass parent_class;
};

GType ventuz_video_src_get_type(void);

GST_ELEMENT_REGISTER_DECLARE(ventuzvideosrc);

G_END_DECLS