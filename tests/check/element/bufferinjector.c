/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (data);
      break;
    default:
      break;
  }
}

static void
fakesink_hand_off (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  GstElement *pipeline = (GstElement *) data;

  gst_element_post_message (data, gst_message_new_eos (GST_OBJECT (pipeline)));
}

GST_START_TEST (audio_test_buffer_injector)
{
  GstElement *pipeline, *audiotestsrc, *bufferinjector, *fakesink;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GstBus *bus;

  pipeline = gst_pipeline_new ("bufferinjector0-test");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), loop);

  /* Create gstreamer elements */
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  bufferinjector = gst_element_factory_make ("bufferinjector", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (fakesink, "sync", TRUE, "signal-handoffs", TRUE, NULL);
  g_object_set (audiotestsrc, "is-live", TRUE, NULL);

  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), pipeline);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, bufferinjector,
      fakesink, NULL);
  gst_element_link (audiotestsrc, bufferinjector);
  gst_element_link (bufferinjector, fakesink);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (GST_OBJECT (bus));
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");
  g_main_loop_unref (loop);
}

GST_END_TEST;

GST_START_TEST (video_test_buffer_injector)
{
  GstElement *pipeline, *videotestsrc, *bufferinjector, *fakesink;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GstBus *bus;

  pipeline = gst_pipeline_new ("bufferinjector0-test");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), loop);

  /* Create gstreamer elements */
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  bufferinjector = gst_element_factory_make ("bufferinjector", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (fakesink, "sync", TRUE, "signal-handoffs", TRUE, NULL);
  g_object_set (videotestsrc, "is-live", TRUE, NULL);

  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), pipeline);

  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, bufferinjector,
      fakesink, NULL);
  gst_element_link (videotestsrc, bufferinjector);
  gst_element_link (bufferinjector, fakesink);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (GST_OBJECT (bus));
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");
  g_main_loop_unref (loop);
}

GST_END_TEST;

GST_START_TEST (buffer_injector_drop_buffers)
{
  GstElement *pipeline, *videotestsrc, *identity, *bufferinjector, *fakesink;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GstBus *bus;

  pipeline = gst_pipeline_new ("bufferinjector0-test");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), loop);

  /* Create gstreamer elements */
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  identity = gst_element_factory_make ("identity", NULL);
  bufferinjector = gst_element_factory_make ("bufferinjector", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (fakesink, "sync", TRUE, "signal-handoffs", TRUE, NULL);
  g_object_set (identity, "sleep-time", 500000, NULL);
  g_object_set (videotestsrc, "is-live", TRUE, NULL);

  g_signal_connect (G_OBJECT (fakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), pipeline);

  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, identity, bufferinjector,
      fakesink, NULL);
  gst_element_link (videotestsrc, identity);
  gst_element_link (identity, bufferinjector);
  gst_element_link (bufferinjector, fakesink);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (GST_OBJECT (bus));
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");
  g_main_loop_unref (loop);
}

GST_END_TEST;

static Suite *
buffer_injector_suite (void)
{
  Suite *s = suite_create ("kmsbufferinjector");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, audio_test_buffer_injector);
  tcase_add_test (tc_chain, video_test_buffer_injector);
  tcase_add_test (tc_chain, buffer_injector_drop_buffers);
  return s;
}

GST_CHECK_MAIN (buffer_injector);
