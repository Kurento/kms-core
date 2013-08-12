#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#define NUM_BUFFERS 3

static GMainLoop *loop = NULL;
static guint buffers_sent = 0;

/* Buffer color white? */
static gboolean white = FALSE;

/* Buffer  timestamp */
static GstClockTime timestamp = 0;

static gboolean
send_buffer_cb (gpointer data)
{
  GstElement *httpep = GST_ELEMENT (data);
  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;

  size = 385 * 288 * 2;

  buffer = gst_buffer_new_allocate (NULL, size, NULL);

  /* this makes the image black/white */
  gst_buffer_memset (buffer, 0, white ? 0xff : 0x0, size);

  white = !white;

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) =
      gst_util_uint64_scale_int (G_GUINT64_CONSTANT (1), GST_SECOND, 2);

  timestamp += GST_BUFFER_DURATION (buffer);

  mark_point ();

  g_signal_emit_by_name (httpep, "push-buffer", buffer, &ret);

  mark_point ();

  if (ret != GST_FLOW_OK) {
    /* something wrong, stop pushing */
    GST_ERROR ("Could not send buffer. Status code %d", ret);
    goto exit;
  }

  GST_INFO ("Sending buffer %d", ++buffers_sent);

  if (buffers_sent < NUM_BUFFERS) {
    /* Do not remove this source yet */
    return TRUE;
  }

exit:
  g_main_loop_quit (loop);
  /* remove source */
  return FALSE;
}

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
      GST_TRACE ("Event: %P", msg);
      break;
    }
    default:
      break;
  }
}

GST_START_TEST (check_push_buffer)
{
  GstElement *pipeline, *httpep, *filesink;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("httpep-test");
  httpep = gst_element_factory_make ("httpendpoint", NULL);
  filesink = gst_element_factory_make ("fakesink", NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), httpep);
  gst_element_set_state (httpep, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), filesink);
  gst_element_set_state (filesink, GST_STATE_PLAYING);

  gst_element_link (httpep, filesink);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "entering_main_loop");

  g_timeout_add (1000, send_buffer_cb, httpep);

  mark_point ();

  g_main_loop_run (loop);

  mark_point ();

  GST_DEBUG ("Main loop stopped");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_main_loop");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));

  g_source_remove (bus_watch_id);
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
