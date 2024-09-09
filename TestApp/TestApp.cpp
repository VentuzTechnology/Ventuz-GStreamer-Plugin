#include <gst/gst.h>
#include <glib/gstdio.h>
static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data)
{
    GMainLoop* loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        g_print("End-of-stream\n");
        g_main_loop_quit(loop);
        break;
    case GST_MESSAGE_ERROR: {
        gchar* debug = NULL;
        GError* err = NULL;

        gst_message_parse_error(msg, &err, &debug);

        g_print("Error: %s\n", err->message);
        g_error_free(err);

        if (debug) {
            g_print("Debug details: %s\n", debug);
            g_free(debug);
        }

        g_main_loop_quit(loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}

gint main(gint argc, gchar* argv[])
{   
    /* initialization */
    gst_init(&argc, &argv);

    // point plugin scanner to our output dir
    const char *exe_dir = g_win32_get_package_installation_directory_of_module(NULL);
    gst_registry_scan_path(gst_registry_get(), exe_dir);

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
   
    /* create elements */
    GstElement* pipeline = gst_pipeline_new("my_pipeline");

    /* watch for messages on the pipeline's bus (note that this will only
     * work like this when a GLib main loop is running) */
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    guint watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    // Let's get some video and audio from Ventuz, 
    // mux it as mpeg-ts with Opus audio, 
    // and send it out over SRT

    GstElement* src = gst_element_factory_make("ventuzvideosrc", "src");
    GstElement* asrc = gst_element_factory_make("ventuzaudiosrc", "asrc");

    GstElement* aenc = gst_element_factory_make("opusenc", "aenc");
    g_object_set(aenc, "bitrate", "128000", NULL);

    GstElement* mux = gst_element_factory_make("mpegtsmux", "mux");
    GstElement* q = gst_element_factory_make("queue", "q");

    GstElement* sink = gst_element_factory_make("srtsink", "sink");
    g_object_set(sink, "uri", "srt://localhost:10001", NULL);
    
    if (!src || !sink || !asrc || !aenc || !mux || !q) {
        g_print("Elements not found\n");
        return -1;
    }
  
    gst_bin_add_many(GST_BIN(pipeline), src, asrc, aenc, mux, q, sink, NULL);

    /* link everything together */
    if (!gst_element_link_many(src, mux, q, sink, NULL)) {
        g_print("Failed to link one or more elements!\n");
        return -1;
    }

    if (!gst_element_link_many(asrc, aenc, mux, NULL))
    {
        g_print("Failed to link one or more elements!\n");
        return -1;
    }

    /* run */
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GstMessage* msg;

        g_print("Failed to start up pipeline!\n");

        /* check if there is an error message with details on the bus */
        msg = gst_bus_poll(bus, GST_MESSAGE_ERROR, 0);
        if (msg) {
            GError* err = NULL;

            gst_message_parse_error(msg, &err, NULL);
            g_print("ERROR: %s\n", err->message);
            g_error_free(err);
            gst_message_unref(msg);
        }
        return -1;
    }

    g_main_loop_run(loop);

    /* clean up */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(watch_id);
    g_main_loop_unref(loop);

    return 0;
}