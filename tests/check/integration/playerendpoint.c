/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */
#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include "kmsuriendpointstate.h"
#include "kmsrecordingprofile.h"

static GstElement *pipeline;

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

static const gchar *
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

static gboolean
stop_recorder (gpointer user_data)
{
  GstElement *recorder = GST_ELEMENT (user_data);

  GST_WARNING ("EOS Received. Stopping recorder %" GST_PTR_FORMAT, recorder);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_stopping_recorder");

  g_object_set (G_OBJECT (recorder), "state", KMS_URI_END_POINT_STATE_STOP,
      NULL);
  return FALSE;
}

static void
player_eos (GstElement * player, gpointer user_data)
{
  g_idle_add (stop_recorder, user_data);
}

static void
recorder_state_changed_cb (GstElement * recorder, KmsUriEndPointState newState,
    gpointer loop)
{
  GST_INFO ("Element %s changed its state to %s.", GST_ELEMENT_NAME (recorder),
      state2string (newState));
  if (newState == KMS_URI_END_POINT_STATE_STOP)
    g_main_loop_quit (loop);
}

static void
player_state_changed_cb (GstElement * recorder, KmsUriEndPointState newState,
    gpointer loop)
{
  GST_INFO ("Element %s changed its state to %s.", GST_ELEMENT_NAME (recorder),
      state2string (newState));
}

GST_START_TEST (check_custom_event)
{
  GstElement *player, *recorder;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("check_custom_event");
  g_object_set (pipeline, "async-handling", TRUE, NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  player = gst_element_factory_make ("playerendpoint", NULL);
  recorder = gst_element_factory_make ("recorderendpoint", NULL);

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (player), "uri",
      "http://ci.kurento.com/downloads/small.webm", NULL);
  g_object_set (G_OBJECT (recorder), "uri",
      "file:///tmp/player_recorder_integration.webm", NULL);

  g_signal_connect (player, "state-changed",
      G_CALLBACK (player_state_changed_cb), loop);

  g_signal_connect (recorder, "state-changed",
      G_CALLBACK (recorder_state_changed_cb), loop);

  gst_bin_add_many (GST_BIN (pipeline), player, recorder, NULL);

  gst_element_link_pads (player, "video_src_0", recorder, "video_sink");
  gst_element_link_pads (player, "audio_src_0", recorder, "audio_sink");

  g_signal_connect (G_OBJECT (player), "eos", G_CALLBACK (player_eos),
      recorder);

  /* Set player and recorder to start state */
  g_object_set (G_OBJECT (player), "state", KMS_URI_END_POINT_STATE_START,
      NULL);
  g_object_set (G_OBJECT (recorder), "state", KMS_URI_END_POINT_STATE_START,
      NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

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
  tcase_add_test (tc_chain, check_custom_event);
  return s;
}

GST_CHECK_MAIN (playerendpoint);
