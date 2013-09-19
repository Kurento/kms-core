#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include "kmsuriendpointstate.h"

static GMainLoop *loop = NULL;
static GstElement *player = NULL;
static GstElement *fakesink = NULL;

static guint state = 0;
static gboolean start_buffer = FALSE;

struct state_controller
{
  KmsUriEndPointState state;
  guint seconds;
};

static const struct state_controller trasnsitions[] = {
  {KMS_URI_END_POINT_STATE_START, 5},
  {KMS_URI_END_POINT_STATE_STOP, 5},
  {KMS_URI_END_POINT_STATE_START, 5}
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
handoff (GstElement * object, GstBuffer * arg0,
    GstPad * arg1, gpointer user_data)
{
  switch (trasnsitions[state].state) {
    case KMS_URI_END_POINT_STATE_STOP:
      GST_DEBUG (" ---------> handoff in STOP state");
      break;
    case KMS_URI_END_POINT_STATE_START:
      start_buffer = TRUE;
      break;
    case KMS_URI_END_POINT_STATE_PAUSE:
      break;
    default:
      break;
  }
}

static void
change_state (KmsUriEndPointState state)
{
  GST_DEBUG ("Setting player to state %s", state2string (state));
  g_object_set (G_OBJECT (player), "state", state, NULL);
}

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{

  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "bus_error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
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
    GST_DEBUG ("All transitions done. Finishing player check states suit");
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

GST_START_TEST (check_states)
{
  GstElement *pipeline;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline_live_stream");
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  player = gst_element_factory_make ("playerendpoint", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (player), "uri",
      "http://ci.kurento.com/downloads/sintel.webm", NULL);
  g_object_set (G_OBJECT (fakesink), "signal-handoffs", TRUE, NULL);
  g_signal_connect (fakesink, "handoff", G_CALLBACK (handoff), loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_main_loop_check_states");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), fakesink);
  gst_element_set_state (fakesink, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), player);
  gst_element_set_state (player, GST_STATE_PLAYING);

  gst_element_link (player, fakesink);

  transite ();

  g_main_loop_run (loop);

  fail_unless (start_buffer == TRUE);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_entering_main_loop_check_states");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST
/* check_live_stream */
static gboolean buffer_audio = FALSE;
static gboolean buffer_video = FALSE;
static GMutex mutex1;

static gboolean
check_handoff_audio (gpointer user_data)
{
  g_mutex_lock (&mutex1);
  buffer_audio = TRUE;
  if (buffer_audio && buffer_video) {

    g_mutex_unlock (&mutex1);
    GMainLoop *loop = (GMainLoop *) user_data;

    g_main_quit (loop);
    return FALSE;
  }
  g_mutex_unlock (&mutex1);
  return FALSE;
}

static gboolean
check_handoff_video (gpointer user_data)
{
  g_mutex_lock (&mutex1);
  buffer_video = TRUE;
  if (buffer_audio && buffer_video) {

    g_mutex_unlock (&mutex1);
    GMainLoop *loop = (GMainLoop *) user_data;

    g_main_quit (loop);
    return FALSE;
  }
  g_mutex_unlock (&mutex1);
  return FALSE;
}

static void
handoff_audio (GstElement * object, GstBuffer * arg0,
    GstPad * arg1, gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  GST_TRACE ("---------------------------->handoff_audio");
  g_idle_add ((GSourceFunc) check_handoff_audio, loop);

}

static void
handoff_video (GstElement * object, GstBuffer * arg0,
    GstPad * arg1, gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  buffer_video = TRUE;
  GST_TRACE ("---------------------------->handoff_video");
  g_idle_add ((GSourceFunc) check_handoff_video, loop);
}

GST_START_TEST (check_live_stream)
{
  GstElement *player, *pipeline;
  GstElement *fakesink_audio, *fakesink_video;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline_live_stream");
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  player = gst_element_factory_make ("playerendpoint", NULL);
  fakesink_audio = gst_element_factory_make ("fakesink", NULL);
  fakesink_video = gst_element_factory_make ("fakesink", NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (player), "uri",
      "http://ci.kurento.com/downloads/sintel.webm", NULL);

  g_object_set (G_OBJECT (fakesink_audio), "signal-handoffs", TRUE, NULL);
  g_signal_connect (fakesink_audio, "handoff", G_CALLBACK (handoff_audio),
      loop);
  g_object_set (G_OBJECT (fakesink_video), "signal-handoffs", TRUE, NULL);
  g_signal_connect (fakesink_video, "handoff", G_CALLBACK (handoff_video),
      loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_main_loop_live_stream");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), fakesink_audio);
  gst_element_set_state (fakesink_audio, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), fakesink_video);
  gst_element_set_state (fakesink_video, GST_STATE_PLAYING);
  gst_bin_add (GST_BIN (pipeline), player);
  gst_element_set_state (player, GST_STATE_PLAYING);

  gst_element_link_pads (player, "audio_src_0", fakesink_audio, "sink");
  gst_element_link_pads (player, "video_src_0", fakesink_video, "sink");

  /* Set player to start state */
  g_object_set (G_OBJECT (player), "state", KMS_URI_END_POINT_STATE_START,
      NULL);

  g_main_loop_run (loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_entering_main_loop_live_stream");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

}

GST_END_TEST
/* check_eos */
    static void
player_eos (GstElement * player, GMainLoop * loop)
{
  GST_DEBUG ("Eos received");
  g_main_loop_quit (loop);
}

/* EOS test */
GST_START_TEST (check_eos)
{
  GstElement *player, *pipeline;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline_live_stream");
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  player = gst_element_factory_make ("playerendpoint", NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (player), "uri",
      "http://ci.kurento.com/downloads/small.webm", NULL);

  gst_bin_add (GST_BIN (pipeline), player);

  g_signal_connect (G_OBJECT (player), "eos", G_CALLBACK (player_eos), loop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Set player to start state */
  g_object_set (G_OBJECT (player), "state", KMS_URI_END_POINT_STATE_START,
      NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_main_loop_live_stream");

  g_main_loop_run (loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_entering_main_loop_live_stream");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

}

GST_END_TEST
/* Define test suite */
static Suite *
playerendpoint_suite (void)
{
  Suite *s = suite_create ("playerendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_states);
  tcase_add_test (tc_chain, check_live_stream);
  tcase_add_test (tc_chain, check_eos);
  return s;
}

GST_CHECK_MAIN (playerendpoint);
