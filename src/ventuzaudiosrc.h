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