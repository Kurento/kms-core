/*
 * agnosticbin.c - gst-kurento-plugins
 *
 * Copyright (C) 2013 Kurento
 * Contact: José Antonio Santos Cadenas <santoscadenas@kurento.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (msg->type) {
    case GST_MESSAGE_ERROR:{
      GST_ERROR ("Error: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      fail ("Error received on bus");
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_WARNING ("Warning: %P", msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      if (g_str_has_prefix (GST_OBJECT_NAME (msg->src), "agnosticbin")) {
        GST_INFO ("Event: %P", msg);
      }
    }
      break;
    default:
      break;
  }
}

static void
negotiated_fakesink_hand_off (GstElement * fakesink, GstBuffer * buf,
    GstPad * pad, gpointer data)
{
  static int count = 0;
  GMainLoop *loop = (GMainLoop *) data;

  if (count++ > 10) {
    g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
    g_main_loop_quit (loop);
  }
}

static void
fakesink_hand_off (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  static int count = 0;
  GMainLoop *loop = (GMainLoop *) data;

  if (count++ > 40) {
    g_object_set (G_OBJECT (fakesink), "signal-handoffs", FALSE, NULL);
    g_main_loop_quit (loop);
  }
}

static gboolean
timeout_check (gpointer pipeline)
{
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "timeout_test_end");
  return FALSE;
}

GST_START_TEST (static_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *decoder = gst_element_factory_make ("vp8dec", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);
  GstElement *outputfakesink = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (fakesink2), "sync", TRUE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink2), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  g_object_set (G_OBJECT (outputfakesink), "signal-handoffs", TRUE, NULL);
  g_signal_connect (G_OBJECT (outputfakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      decoder, fakesink2, NULL);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);
  mark_point ();
  ret = gst_element_link_many (agnosticbin, decoder, fakesink2, NULL);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_timeout_add_seconds (10, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "loopback_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
}

GST_END_TEST
GST_START_TEST (live_link)
{
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
  GstElement *agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  GstElement *decoder = gst_element_factory_make ("rtpmp4vpay", NULL);
  GstElement *outputfakesink = gst_element_factory_make ("fakesink", NULL);
  gboolean ret;

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (fakesink), "sync", TRUE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (negotiated_fakesink_hand_off), loop);

  g_object_set (G_OBJECT (outputfakesink), "signal-handoffs", TRUE, "sync",
      TRUE, NULL);
  g_signal_connect (G_OBJECT (outputfakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), loop);

  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, agnosticbin, fakesink,
      NULL);
  mark_point ();
  gst_element_set_state (fakesink, GST_STATE_PLAYING);
  gst_element_set_state (agnosticbin, GST_STATE_PLAYING);
  mark_point ();
  ret = gst_element_link_many (videotestsrc, agnosticbin, fakesink, NULL);
  fail_unless (ret);

  gst_element_set_state (videotestsrc, GST_STATE_PLAYING);

  g_timeout_add_seconds (30, timeout_check, pipeline);

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  gst_element_set_state (decoder, GST_STATE_PLAYING);
  gst_element_set_state (outputfakesink, GST_STATE_PLAYING);
  gst_bin_add_many (GST_BIN (pipeline), decoder, outputfakesink, NULL);

  mark_point ();
  ret = gst_element_link (decoder, outputfakesink);
  fail_unless (ret);
  ret = gst_element_link (agnosticbin, decoder);
  fail_unless (ret);
  mark_point ();
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop");

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "loopback_test_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
agnostic_suite (void)
{
  Suite *s = suite_create ("agnosticbin");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, live_link);
  tcase_add_test (tc_chain, static_link);

  return s;
}

GST_CHECK_MAIN (agnostic);
