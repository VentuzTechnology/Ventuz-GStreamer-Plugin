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

#include <gst/gst.h>
#include "ventuzaudiosrc.h"
#include "ventuzvideosrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_ventuzplugin_debug);
#define GST_CAT_DEFAULT gst_ventuzplugin_debug

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean plugin_init(GstPlugin* plugin)
{
    /* debug category for filtering log messages
     *
     * exchange the string 'Template plugin' with your description
     */
    GST_DEBUG_CATEGORY_INIT(gst_ventuzplugin_debug, "ventuzplugin",
        0, "Ventuz plugin");

    gboolean ret = GST_ELEMENT_REGISTER(ventuzvideosrc, plugin);
    ret &= GST_ELEMENT_REGISTER(ventuzaudiosrc, plugin);

    return ret;
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "ventuzvideoplugin"
#endif

#define PACKAGE_VERSION 
 /* gstreamer looks for this structure to register plugins
    *
    * exchange the string 'Template plugin' with your plugin description
    */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ventuzvideoplugin,
    "Ventuz Stream Out plugin",
    plugin_init,
    "0.1.0",
    "LGPL",
    "Ventuz",
    "http://ventuz.com"
)
