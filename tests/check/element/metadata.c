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
#include <time.h>

#include "kmsbufferlacentymeta.h"

static gboolean
quit_main_loop (gpointer data)
{
  g_main_loop_quit ((GMainLoop *) data);

  return G_SOURCE_REMOVE;
}

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;
      gchar *err_str;

      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "bus_error");
      gst_message_parse_error (msg, &err, &dbg_info);

      err_str = g_strdup_printf ("Error received on bus: %s: %s", err->message,
          dbg_info);
      g_error_free (err);
      g_free (dbg_info);

      fail (err_str);
      g_free (err_str);

      break;
    }
    case GST_MESSAGE_STATE_CHANGED:{
      GST_TRACE ("Event: %" GST_PTR_FORMAT, msg);
      break;
    }
    default:
      break;
  }
}

static GstPadProbeReturn
add_metadata_data (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buffer;
  struct timespec ts;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0) {
    GST_ERROR_OBJECT (pad, "Can not get time");
    return GST_PAD_PROBE_OK;
  }

  kms_buffer_add_buffer_latency_meta (buffer, GST_TIMESPEC_TO_TIME (ts));

  return GST_PAD_PROBE_OK;
}

static void
debug_latency_meta (GstPad * pad, GstBuffer * buffer)
{
  KmsBufferLatencyMeta *meta;

  meta = kms_buffer_get_buffer_latency_meta (buffer);
  fail_if (meta == NULL);
  fail_if (!GST_CLOCK_TIME_IS_VALID (meta->ts));

  GST_DEBUG_OBJECT (pad, "Meta %" G_GUINT64_FORMAT, meta->ts);
}

static void
calculate_latency (GstPad * pad, GstBuffer * buffer)
{
  KmsBufferLatencyMeta *meta;
  struct timespec ts;

  fail_if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0);

  meta = kms_buffer_get_buffer_latency_meta (buffer);
  fail_if (meta == NULL);
  fail_if (!GST_CLOCK_TIME_IS_VALID (meta->ts));

  GST_INFO_OBJECT (pad, "Latency %" G_GINT64_FORMAT,
      GST_CLOCK_DIFF (meta->ts, GST_TIMESPEC_TO_TIME (ts)));
}

static GstPadProbeReturn
show_metadata_data (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buffer;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  debug_latency_meta (pad, buffer);

  return GST_PAD_PROBE_OK;
}

static void
handoff_cb (GstElement * fakesink, GstBuffer * buff, GstPad * pad,
    gpointer user_data)
{

  calculate_latency (pad, buff);

  g_idle_add ((GSourceFunc) quit_main_loop, user_data);
  g_signal_handlers_disconnect_by_data (fakesink, user_data);
}

GST_START_TEST (check_metadata)
{
  GstElement *pipeline, *videotestsrc, *videoconvert, *fakesink;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;
  GstPad *pad;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("metadata-test");
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  videoconvert = gst_element_factory_make ("videoconvert", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);
  g_object_set (G_OBJECT (fakesink), "sync", FALSE, "async", FALSE,
      "signal-handoffs", TRUE, NULL);

  g_signal_connect (fakesink, "handoff", G_CALLBACK (handoff_cb), loop);

  pad = gst_element_get_static_pad (videotestsrc, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) add_metadata_data, NULL, NULL);
  g_object_unref (pad);

  pad = gst_element_get_static_pad (videoconvert, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) show_metadata_data, NULL, NULL);
  g_object_unref (pad);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, videoconvert,
      fakesink, NULL);
  gst_element_link_many (videotestsrc, videoconvert, fakesink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipe released");

  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST
GST_START_TEST (check_metadata_enc)
{
  GstElement *pipeline, *videotestsrc, *videoconvert, *videoenc, *videodec,
      *fakesink;
  guint bus_watch_id;
  GMainLoop *loop;
  GstBus *bus;
  GstPad *pad;

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("metadata-test");
  videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  videoconvert = gst_element_factory_make ("videoconvert", NULL);
  videoenc = gst_element_factory_make ("x264enc", NULL);
  videodec = gst_element_factory_make ("avdec_h264", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (G_OBJECT (videotestsrc), "is-live", TRUE, NULL);
  g_object_set (G_OBJECT (fakesink), "sync", FALSE, "async", FALSE,
      "signal-handoffs", TRUE, NULL);

  g_signal_connect (fakesink, "handoff", G_CALLBACK (handoff_cb), loop);

  pad = gst_element_get_static_pad (videotestsrc, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) add_metadata_data, NULL, NULL);
  g_object_unref (pad);

  pad = gst_element_get_static_pad (videoconvert, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) show_metadata_data, NULL, NULL);
  g_object_unref (pad);

  pad = gst_element_get_static_pad (videoenc, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) show_metadata_data, NULL, NULL);
  g_object_unref (pad);

  pad = gst_element_get_static_pad (videodec, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) show_metadata_data, NULL, NULL);
  g_object_unref (pad);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, videoconvert,
      videoenc, videodec, fakesink, NULL);
  gst_element_link_many (videotestsrc, videoconvert, videoenc, videodec,
      fakesink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipe released");

  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST
/******************************/
/* metadata test suite        */
/******************************/
static Suite *
metadata_suite (void)
{
  Suite *s = suite_create ("metadata");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  if (FALSE) {
    tcase_add_test (tc_chain, check_metadata);
  }
  tcase_add_test (tc_chain, check_metadata_enc);

  return s;
}

GST_CHECK_MAIN (metadata);
