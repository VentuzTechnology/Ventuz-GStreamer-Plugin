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

    static const int MAX_Q = 5;
};

struct VentuzVideoSrcClass
{
    GstPushSrcClass parent_class;
};

GType ventuz_video_src_get_type(void);

GST_ELEMENT_REGISTER_DECLARE(ventuzvideosrc);

G_END_DECLS