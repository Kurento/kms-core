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
#include <glib.h>
#include "kmsuriendpointstate.h"

#include <kmstestutils.h>

GMainLoop *loop;

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
    case GST_MESSAGE_EOS:{
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GST_ERROR ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      fail ("Warning received on bus");
      break;
    }
    default:
      break;
  }
}

GST_START_TEST (integration)
{
  GstElement *player, *pipeline, *vp8enc, *agnosticbin1, *filter, *agnosticbin2,
      *vp8dec;
  GstElement *fakesink_audio, *fakesink_video;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new ("pipeline_live_stream");
  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);
  player = gst_element_factory_make ("playerendpoint", NULL);
  vp8enc = gst_element_factory_make ("vp8enc", NULL);
  agnosticbin1 = gst_element_factory_make ("agnosticbin", NULL);
  filter = gst_element_factory_make ("jackvader", NULL);
  agnosticbin2 = gst_element_factory_make ("agnosticbin", NULL);
  vp8dec = gst_element_factory_make ("vp8dec", NULL);
  fakesink_audio = gst_element_factory_make ("fakesink", NULL);
  fakesink_video = gst_element_factory_make ("fakesink", NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (player), "uri",
      "http://ci.kurento.com/downloads/small.webm", NULL);

  g_object_set (G_OBJECT (vp8enc), "deadline", 200000, "threads", 1,
      "cpu-used", 16, "resize-allowed", TRUE,
      "target-bitrate", 300000, "end-usage", 1, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), vp8enc);
  gst_element_set_state (vp8enc, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), agnosticbin1);
  gst_element_set_state (agnosticbin1, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), filter);
  gst_element_set_state (filter, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), agnosticbin2);
  gst_element_set_state (agnosticbin2, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), vp8dec);
  gst_element_set_state (vp8dec, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), fakesink_audio);
  gst_element_set_state (fakesink_audio, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), fakesink_video);
  gst_element_set_state (fakesink_video, GST_STATE_PLAYING);

  gst_bin_add (GST_BIN (pipeline), player);
  gst_element_set_state (player, GST_STATE_PLAYING);

  kms_element_link_pads (player, "audio_src_%u", fakesink_audio, "sink");
  kms_element_link_pads (player, "video_src_%u", vp8enc, "sink");

  gst_element_link_many (vp8enc, agnosticbin1, filter, agnosticbin2, vp8dec,
      fakesink_video, NULL);

  /* Set player to start state */
  g_object_set (G_OBJECT (player), "state", KMS_URI_ENDPOINT_STATE_START, NULL);

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

}

GST_END_TEST static Suite *
playerwithfilter_suite (void)
{
  Suite *s = suite_create ("playerwithfilter");
  TCase *tc_chain = tcase_create ("intregration");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, integration);

  return s;
}

GST_CHECK_MAIN (playerwithfilter);
