#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#include "kmsuriendpointstate.h"

gboolean set_state_start (gpointer *);
gboolean set_state_pause (gpointer *);
gboolean set_state_stop (gpointer *);

static GMainLoop *loop = NULL;
static GstElement *recorder = NULL;
static guint state = 0;

struct state_controller
{
  KmsUriEndPointState state;
  guint seconds;
};

static const struct state_controller trasnsitions[] = {
  {KMS_URI_END_POINT_STATE_START, 2},
  {KMS_URI_END_POINT_STATE_START, 3},
  {KMS_URI_END_POINT_STATE_PAUSE, 2},
  {KMS_URI_END_POINT_STATE_START, 2},
  {KMS_URI_END_POINT_STATE_STOP, 3},
  {KMS_URI_END_POINT_STATE_START, 2},
  {KMS_URI_END_POINT_STATE_STOP, 2},
  {KMS_URI_END_POINT_STATE_START, 2}
};

static gchar *
state2string (KmsUriEndPointState state)
{
  switch (state) {
    case KMS_URI_END_POINT_STATE_STOP:
      return "STOP";
    case KMS_URI_END_POINT_STATE_START:
      return "START";
    case KMS_URI_END_POINT_STATE_PAUSE:
      return "PAUSE";
    default:
      return "Invalid state";
  }
}

static void
change_state (KmsUriEndPointState state)
{
  GST_DEBUG ("Setting recorder to state %s", state2string (state));
  g_object_set (G_OBJECT (recorder), "state", state, NULL);
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
      GST_INFO ("Event: %P", msg);
      break;
    }
    default:
      break;
  }
}

static gboolean transite_cb (gpointer);

static void
transite ()
{
  if (state < G_N_ELEMENTS (trasnsitions)) {
    change_state (trasnsitions[state].state);
    g_timeout_add (trasnsitions[state].seconds * 1000, transite_cb, NULL);
  } else {
    GST_DEBUG ("All transitions done. Finishing recorder test suit");
    g_main_loop_quit (loop);
  }
}

static gboolean
transite_cb (gpointer data)
{
  state++;
  transite ();
  return FALSE;
}

GST_START_TEST (check_states_pipeline)
{
  GstElement *pipeline, *videotestsrc, *audiotestsrc, *timeoverlay;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("recorderendpoint0-test");
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  timeoverlay = gst_element_factory_make ("timeoverlay", NULL);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  recorder = gst_element_factory_make ("recorderendpoint", NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, videotestsrc, recorder,
      timeoverlay, NULL);
  gst_element_link (videotestsrc, timeoverlay);
  gst_element_link_pads (timeoverlay, "src", recorder, "video_sink");
  gst_element_link_pads (audiotestsrc, "src", recorder, "audio_sink");

  g_object_set (G_OBJECT (recorder), "uri", "file:///tmp/state_recorder_%u.avi",
      NULL);
  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, "do-timestamp", TRUE,
      "pattern", 18, NULL);
  g_object_set (G_OBJECT (audiotestsrc), "is-live", TRUE, "do-timestamp", TRUE,
      NULL);
  g_object_set (G_OBJECT (timeoverlay), "font-desc", "Sans 28", NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before entering main loop");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  transite ();

  g_main_loop_run (loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after entering main loop");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST
/******************************/
/* RecorderEndPoint test suit */
/******************************/
static Suite *
recorderendpoint_suite (void)
{
  Suite *s = suite_create ("recorderendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_states_pipeline);
  return s;
}

GST_CHECK_MAIN (recorderendpoint);
