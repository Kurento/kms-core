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

#define AUDIO_SINK audio_
#define VIDEO_SINK video_

typedef struct _CustomData
{
  GstElement *audiotestsrc;
  GstElement *videotestsrc;
  GstElement *encoder;
  GstElement *automuxerbin;
  GMainLoop *loop;
} CustomData;

static GstElement *pipeline = NULL;
static gchar *test_name = NULL;

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (msg->type) {
    case GST_MESSAGE_ERROR:
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "bus_error");
      fail ("Error received on bus");
      break;
    case GST_MESSAGE_WARNING:
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    case GST_MESSAGE_STATE_CHANGED:
      GST_TRACE ("Event: %" GST_PTR_FORMAT, msg);
      break;
    default:
      break;
  }
}

void
pad_added (GstElement * element, GstPad * pad)
{
  static guint count = 0;

  if (!GST_PAD_IS_SRC (pad)) {
    GST_DEBUG ("Sink pad %s ignored", gst_pad_get_name (pad));
    return;
  }

  GST_DEBUG ("Pad_added callback");

  GstElement *file = gst_element_factory_make ("filesink", NULL);
  gchar *name = g_strdup_printf ("/tmp/%s_%d.avi", test_name, count);

  g_object_set (G_OBJECT (file), "location", name, NULL);
  gst_bin_add (GST_BIN (pipeline), file);
  gst_element_sync_state_with_parent (file);

  GstPad *sinkpad = gst_element_get_static_pad (file, "sink");

  if (gst_pad_link (pad, sinkpad) != GST_PAD_LINK_OK)
    fail ("Error linking automuxerbin and filesink");
  else
    GST_DEBUG ("LINK srcpad_automuxer with sinkpad filesink");

  count++;
  gst_object_unref (sinkpad);
}

void
pad_removed (GstElement * element, GstPad * pad, gpointer data)
{
  /* Empty function. Used just for testing purposes about signal handling */
  GST_DEBUG ("-----%s-------->pad_removed", gst_pad_get_name (pad));
}

static gboolean
timer (CustomData * data)
{
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "finished timer");
  g_main_loop_quit (data->loop);
  return FALSE;
}

GST_START_TEST (audio_video_raw)
{
  CustomData data;

  test_name = "audio_video_raw";

  data.loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);
  data.videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  data.audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  data.automuxerbin = gst_element_factory_make ("automuxerbin", NULL);

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (data.videotestsrc), "num-buffers", 250, NULL);
  g_object_set (G_OBJECT (data.audiotestsrc), "num-buffers", 440, NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), data.audiotestsrc,
      data.videotestsrc, data.automuxerbin, NULL);

  g_signal_connect (data.automuxerbin, "pad-added", G_CALLBACK (pad_added),
      NULL);
  g_signal_connect (data.automuxerbin, "pad-removed", G_CALLBACK (pad_removed),
      NULL);

  /* Manually link the automuxer, which has "Request" pads */
  if (!gst_element_link_pads (data.videotestsrc, "src", data.automuxerbin,
          "video_0")) {
    fail ("automuxer could not be linked.");
    g_main_loop_quit (data.loop);
    gst_object_unref (pipeline);
  }
  if (!gst_element_link_pads (data.audiotestsrc, "src", data.automuxerbin,
          "audio_0")) {
    fail ("automuxer could not be linked.");
    g_main_loop_quit (data.loop);
    gst_object_unref (pipeline);
  }

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop_audio_video_raw");

  g_timeout_add (5000, (GSourceFunc) timer, &data);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  mark_point ();
  g_main_loop_run (data.loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "create_buffer_test_end_audio_video_raw");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
  g_main_loop_unref (data.loop);
}

GST_END_TEST
GST_START_TEST (vp8enc)
{
  CustomData data;

  test_name = "vp8enc";

  data.loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);
  data.videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  data.encoder = gst_element_factory_make ("vp8enc", NULL);
  data.audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  data.automuxerbin = gst_element_factory_make ("automuxerbin", NULL);

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (data.videotestsrc), "num-buffers", 250, NULL);
  g_object_set (G_OBJECT (data.audiotestsrc), "num-buffers", 440, NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), data.audiotestsrc,
      data.videotestsrc, data.encoder, data.automuxerbin, NULL);
  gst_element_link (data.videotestsrc, data.encoder);

  g_signal_connect (data.automuxerbin, "pad-added", G_CALLBACK (pad_added),
      NULL);
  g_signal_connect (data.automuxerbin, "pad-removed", G_CALLBACK (pad_removed),
      NULL);

  /* Manually link the automuxer, which has "Request" pads */
  if (!gst_element_link_pads (data.encoder, "src", data.automuxerbin,
          "video_0")) {
    fail ("automuxer could not be linked.");
    g_main_loop_quit (data.loop);
    gst_object_unref (pipeline);
  }
  if (!gst_element_link_pads (data.audiotestsrc, "src", data.automuxerbin,
          "audio_0")) {
    fail ("automuxer could not be linked.");
    g_main_loop_quit (data.loop);
    gst_object_unref (pipeline);
  }

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop_audio_video_raw");
  g_timeout_add (5000, (GSourceFunc) timer, &data);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  mark_point ();
  g_main_loop_run (data.loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "create_buffer_test_end_audio_video_raw");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
  g_main_loop_unref (data.loop);
}

GST_END_TEST static gboolean
timer_video_audio (CustomData * data)
{
  data->audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (G_OBJECT (data->audiotestsrc), "num-buffers", 440, NULL);
  gst_bin_add (GST_BIN (pipeline), data->audiotestsrc);

  if (!gst_element_link_pads (data->audiotestsrc, "src", data->automuxerbin,
          "audio_0")) {
    fail ("audiotestsrc--automuxer could not be linked.");
    g_main_loop_quit (data->loop);
    gst_object_unref (pipeline);
  }
  gst_element_sync_state_with_parent (data->audiotestsrc);
  return FALSE;
}

GST_START_TEST (delay_audio)
{
  CustomData data;

  test_name = "delay_audio";

  data.loop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);
  data.videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  data.encoder = gst_element_factory_make ("vp8enc", NULL);
  data.automuxerbin = gst_element_factory_make ("automuxerbin", NULL);

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  g_object_set (G_OBJECT (data.videotestsrc), "num-buffers", 250, NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline), data.videotestsrc, data.encoder,
      data.automuxerbin, NULL);
  gst_element_link (data.videotestsrc, data.encoder);

  g_signal_connect (data.automuxerbin, "pad-added", G_CALLBACK (pad_added),
      NULL);
  g_signal_connect (data.automuxerbin, "pad-removed", G_CALLBACK (pad_removed),
      NULL);

  /* Manually link the automuxer, which has "Request" pads */
  if (!gst_element_link_pads (data.encoder, "src", data.automuxerbin,
          "video_0")) {
    fail ("automuxer could not be linked.");
    g_main_loop_quit (data.loop);
    gst_object_unref (pipeline);
  }

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop_audio_video_raw");
  g_timeout_add (2000, (GSourceFunc) timer_video_audio, &data);
  g_timeout_add (5000, (GSourceFunc) timer, &data);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  mark_point ();
  g_main_loop_run (data.loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "create_buffer_test_end_audio_video_raw");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
  g_main_loop_unref (data.loop);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
automuxer_suite (void)
{
  Suite *s = suite_create ("automuxerbin");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, audio_video_raw);
  tcase_add_test (tc_chain, vp8enc);
  tcase_add_test (tc_chain, delay_audio);

  return s;
}

GST_CHECK_MAIN (automuxer);
