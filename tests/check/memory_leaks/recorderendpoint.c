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

#include <kmscheck.h>
#include "kmsuriendpointstate.h"

#define ITERATIONS 1

static int iterations = ITERATIONS;

static GMainLoop *loop;

#define RECORDER_NAME "recorder"

static gboolean
quit_main_loop_idle (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);
  return FALSE;
}

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_EOS:{
      GstElement *recorderendpoint;

      GST_DEBUG ("EOS received");
      recorderendpoint = gst_bin_get_by_name (GST_BIN (pipe), RECORDER_NAME);
      g_object_set (G_OBJECT (recorderendpoint), "state",
          KMS_URI_END_POINT_STATE_STOP, NULL);
      g_object_unref (recorderendpoint);
      break;
    }
    default:
      break;
  }
}

static void
create_element (const gchar * element_name)
{
  GstElement *element = gst_element_factory_make (element_name, NULL);

  g_object_unref (element);
}

static void
play_element (const gchar * element_name)
{
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *element = gst_element_factory_make (element_name, NULL);

  gst_bin_add (GST_BIN (pipeline), element);

  gst_element_set_state (pipeline, GST_STATE_READY);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_object_unref (pipeline);
}

static void
recorder_stopped (GstElement * recorder, gpointer user_data)
{
  GST_INFO ("Recorder stopped signal");
  quit_main_loop_idle (loop);
}

static void
start_audio_recorderendpoint ()
{
  gboolean ret;
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *recorderendpoint =
      gst_element_factory_make ("recorderendpoint", RECORDER_NAME);

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (audiotestsrc), "num-buffers", 50, "is-live", TRUE,
      "do-timestamp", TRUE, "wave", 8, NULL);
  g_object_set (G_OBJECT (recorderendpoint), "uri",
      "file:///tmp/audio_recorder_%u.avi", NULL);
  g_signal_connect (recorderendpoint, "stopped", G_CALLBACK (recorder_stopped),
      NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, agnosticbin,
      recorderendpoint, NULL);
  mark_point ();
  ret = gst_element_link (audiotestsrc, agnosticbin);
  fail_unless (ret);
  mark_point ();
  ret =
      gst_element_link_pads (agnosticbin, NULL, recorderendpoint, "audio_sink");
  fail_unless (ret);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();
  g_object_set (G_OBJECT (recorderendpoint), "state",
      KMS_URI_END_POINT_STATE_START, NULL);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_main_loop_unref (loop);
  g_object_unref (pipeline);
}

static void
start_video_recorderendpoint ()
{
  gboolean ret;
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *recorderendpoint =
      gst_element_factory_make ("recorderendpoint", RECORDER_NAME);

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (videotestsrc), "num-buffers", 50, "is-live", TRUE,
      "do-timestamp", TRUE, "pattern", 18, NULL);
  g_object_set (G_OBJECT (recorderendpoint), "uri",
      "file:///tmp/video_recorder_%u.avi", NULL);
  g_signal_connect (recorderendpoint, "stopped", G_CALLBACK (recorder_stopped),
      NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin,
      recorderendpoint, NULL);
  mark_point ();
  ret = gst_element_link (videotestsrc, agnosticbin);
  fail_unless (ret);
  mark_point ();
  ret =
      gst_element_link_pads (agnosticbin, NULL, recorderendpoint, "video_sink");
  fail_unless (ret);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();
  g_object_set (G_OBJECT (recorderendpoint), "state",
      KMS_URI_END_POINT_STATE_START, NULL);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_main_loop_unref (loop);
  g_object_unref (pipeline);
}

static void
start_audio_video_recorderendpoint ()
{
  gboolean ret;
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *audio_agnosticbin =
      gst_element_factory_make ("agnosticbin", NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *video_agnosticbin =
      gst_element_factory_make ("agnosticbin", NULL);
  GstElement *recorderendpoint =
      gst_element_factory_make ("recorderendpoint", RECORDER_NAME);

  loop = g_main_loop_new (NULL, TRUE);

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

  g_object_set (G_OBJECT (audiotestsrc), "num-buffers", 50, "is-live", TRUE,
      "do-timestamp", TRUE, "wave", 8, NULL);
  g_object_set (G_OBJECT (videotestsrc), "num-buffers", 50, "is-live", TRUE,
      "do-timestamp", TRUE, "pattern", 18, NULL);
  g_object_set (G_OBJECT (recorderendpoint), "uri",
      "file:///tmp/audio_video_recorder_%u.avi", NULL);
  g_signal_connect (recorderendpoint, "stopped", G_CALLBACK (recorder_stopped),
      NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, audio_agnosticbin,
      videotestsrc, video_agnosticbin, recorderendpoint, NULL);
  mark_point ();

  // Link audio elements
  ret = gst_element_link (audiotestsrc, audio_agnosticbin);
  fail_unless (ret);
  mark_point ();
  ret =
      gst_element_link_pads (audio_agnosticbin, NULL, recorderendpoint,
      "audio_sink");
  fail_unless (ret);
  mark_point ();

  // Link video elements
  ret = gst_element_link (videotestsrc, video_agnosticbin);
  fail_unless (ret);
  mark_point ();
  ret =
      gst_element_link_pads (video_agnosticbin, NULL, recorderendpoint,
      "video_sink");
  fail_unless (ret);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();
  g_object_set (G_OBJECT (recorderendpoint), "state",
      KMS_URI_END_POINT_STATE_START, NULL);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_main_loop_unref (loop);
  g_object_unref (pipeline);
}

GST_START_TEST (test_create_recorderendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    create_element ("recorderendpoint");
  }
}

KMS_END_TEST
GST_START_TEST (test_play_recorderendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    play_element ("recorderendpoint");
  }
}

KMS_END_TEST
GST_START_TEST (test_start_audio_recorderendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    start_audio_recorderendpoint ();
  }
}

KMS_END_TEST
GST_START_TEST (test_start_video_recorderendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    start_video_recorderendpoint ();
  }
}

KMS_END_TEST
GST_START_TEST (test_start_audio_video_recorderendpoint)
{
  int i;

  for (i = 0; i < iterations; i++) {
    start_audio_video_recorderendpoint ();
  }
}

KMS_END_TEST
/*
 * End of test cases
 */
static Suite *
recorderendpoint_suite (void)
{
  char *it_str;

  it_str = getenv ("ITERATIONS");
  if (it_str != NULL) {
    iterations = atoi (it_str);
    if (iterations <= 0)
      iterations = ITERATIONS;
  }

  Suite *s = suite_create ("recorderendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_create_recorderendpoint);
  tcase_add_test (tc_chain, test_play_recorderendpoint);
  tcase_add_test (tc_chain, test_start_audio_recorderendpoint);
  tcase_add_test (tc_chain, test_start_video_recorderendpoint);
  tcase_add_test (tc_chain, test_start_audio_video_recorderendpoint);

  return s;
}

KMS_CHECK_MAIN (recorderendpoint);
