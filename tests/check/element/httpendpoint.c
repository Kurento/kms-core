#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

//#define LOCATION "http://ci.kurento.com/downloads/sintel_trailer-480p.webm"
#define LOCATION "http://ci.kurento.com/downloads/small.webm"

static GMainLoop *loop = NULL;
GstElement *src_pipeline, *souphttpsrc, *appsink;
GstElement *test_pipeline, *httpep, *filesink;

static void
src_bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{

  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Source bus error: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "source_bus_error");
      fail ("Error received on source bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Source bus: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:{
      GST_TRACE ("Source bus event: %P", msg);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG ("Source bus: End of stream\n");
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
}

static void
test_bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{

  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Test bus error: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "test_bus_error");
      fail ("Error received on test bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Test bus: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:{
      GST_TRACE ("Test bus event: %P", msg);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG ("Test bus_ End of stream\n");
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
}

static void
recv_sample (GstElement * appsink, gpointer user_data)
{
  GstFlowReturn ret;
  GstSample *sample;
  GstBuffer *buffer;

  g_signal_emit_by_name (appsink, "pull-sample", &sample);
  if (sample == NULL) {
    GST_ERROR ("No sample received");
    return;
  }

  buffer = gst_sample_get_buffer (sample);
  if (buffer == NULL) {
    GST_ERROR ("No buffer received");
    return;
  }

  buffer->pts = G_GUINT64_CONSTANT (0);
  buffer->dts = G_GUINT64_CONSTANT (0);

  buffer->offset = G_GUINT64_CONSTANT (0);
  buffer->offset_end = G_GUINT64_CONSTANT (0);
  g_signal_emit_by_name (httpep, "push-buffer", buffer, &ret);

  if (ret != GST_FLOW_OK) {
    /* something wrong */
    GST_ERROR ("Could not send buffer to httpep %s. Ret code %d", ret,
        GST_ELEMENT_NAME (httpep));
  }
}

GST_START_TEST (check_push_buffer)
{
  guint bus_watch_id1, bus_watch_id2;
  GstBus *srcbus, *testbus;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create source pipeline */
  src_pipeline = gst_pipeline_new ("src-pipeline");
  souphttpsrc = gst_element_factory_make ("souphttpsrc", NULL);
  appsink = gst_element_factory_make ("appsink", NULL);

  srcbus = gst_pipeline_get_bus (GST_PIPELINE (src_pipeline));

  bus_watch_id1 = gst_bus_add_watch (srcbus, gst_bus_async_signal_func, NULL);
  g_signal_connect (srcbus, "message", G_CALLBACK (src_bus_msg), src_pipeline);
  g_object_unref (srcbus);

  gst_bin_add_many (GST_BIN (src_pipeline), souphttpsrc, appsink, NULL);
  gst_element_link (souphttpsrc, appsink);

  /* configure objects */
  g_object_set (G_OBJECT (souphttpsrc), "location", LOCATION,
      "is-live", TRUE, "do-timestamp", TRUE, NULL);

  g_object_set (appsink, "emit-signals", TRUE, NULL);
  g_signal_connect (appsink, "new-sample", G_CALLBACK (recv_sample), NULL);

  /* Create test pipeline */
  test_pipeline = gst_pipeline_new ("test-pipeline");
  httpep = gst_element_factory_make ("httpendpoint", NULL);
  filesink = gst_element_factory_make ("filesink", NULL);

  testbus = gst_pipeline_get_bus (GST_PIPELINE (test_pipeline));

  bus_watch_id2 = gst_bus_add_watch (testbus, gst_bus_async_signal_func, NULL);
  g_signal_connect (testbus, "message", G_CALLBACK (test_bus_msg),
      test_pipeline);
  g_object_unref (testbus);

  gst_bin_add_many (GST_BIN (test_pipeline), httpep, filesink, NULL);
  gst_element_link (httpep, filesink);

  g_object_set (G_OBJECT (filesink), "location", "/tmp/test.webm", NULL);

  /* Set pipeline to start state */
  gst_element_set_state (test_pipeline, GST_STATE_PLAYING);
  gst_element_set_state (src_pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (src_pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "entering_main_loop");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (test_pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "entering_main_loop");

  mark_point ();

  g_main_loop_run (loop);

  mark_point ();

  GST_DEBUG ("Main loop stopped");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (src_pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_main_loop");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (test_pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_main_loop");

  gst_element_set_state (src_pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (src_pipeline));

  gst_element_set_state (test_pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (test_pipeline));

  g_source_remove (bus_watch_id1);
  g_source_remove (bus_watch_id2);
  g_main_loop_unref (loop);
}

GST_END_TEST
/******************************/
/* HttpEndPoint test suit */
/******************************/
static Suite *
httpendpoint_suite (void)
{
  Suite *s = suite_create ("httpendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_push_buffer);
  return s;
}

GST_CHECK_MAIN (httpendpoint);
