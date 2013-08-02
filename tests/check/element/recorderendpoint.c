#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#include "kmsuriendpointstate.h"

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{

  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "bus_error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:{
      GST_INFO ("Event: %P", msg);
      break;
    }
    default:
      break;
  }
}

static gboolean
timer (gpointer * data)
{
  GMainLoop *loop = (GMainLoop *) data;

  g_main_loop_quit (loop);

  return FALSE;
}

GST_START_TEST (check_pipeline)
{
  GstElement *pipeline, *videotestsrc, *audiotestsrc, *recorder;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("recorderendpoint-test");
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  recorder = gst_element_factory_make ("recorderendpoint", NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, videotestsrc, recorder,
      NULL);
  gst_element_link_pads (videotestsrc, "src", recorder, "video_sink");
  gst_element_link_pads (audiotestsrc, "src", recorder, "audio_sink");

  g_object_set (G_OBJECT (videotestsrc), "num-buffers", 400, NULL);
  g_object_set (G_OBJECT (audiotestsrc), "num-buffers", 400, NULL);
  g_object_set (G_OBJECT (recorder), "uri", "recorder_test.avi", NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before entering main loop");

  g_timeout_add (5000, (GSourceFunc) timer, loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Set recorder to start state */
  g_object_set (G_OBJECT (recorder), "state", KMS_URI_END_POINT_STATE_START,
      NULL);

  g_main_loop_run (loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after entering main loop");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST static Suite *
recorderendpoint_suite (void)
{
  Suite *s = suite_create ("recorderendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_pipeline);

  return s;
}

GST_CHECK_MAIN (recorderendpoint);
