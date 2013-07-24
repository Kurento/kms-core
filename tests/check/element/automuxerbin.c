/*
 * automuxerbin.c - gst-kurento-plugins
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

static gboolean test_timeout = FALSE;
GstElement *pipeline0;

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
    case GST_MESSAGE_STATE_CHANGED:
    {
      if (g_str_has_prefix (GST_OBJECT_NAME (msg->src), "automuxerbin")
          || g_str_has_prefix (GST_OBJECT_NAME (msg->src), "filesink")) {
        GST_INFO ("Event: %P", msg);
      }
    }
      break;
    default:
      break;
  }
}

static gboolean
timeout (gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  GST_DEBUG ("finished timeout");

  test_timeout = FALSE;
  //GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
  //  GST_DEBUG_GRAPH_SHOW_ALL, "finished timeout");
  g_main_loop_quit (loop);

  return FALSE;
}

void
pad_added (GstElement * element, GstPad * pad, gpointer data)
{
  GstPad *sinkpad;
  GstElement *filesink = (GstElement *) data;

  GST_DEBUG ("Pad_added callback");
  if (!GST_PAD_IS_SRC (pad)) {
    GST_DEBUG ("Sink pad %s ignored", gst_pad_get_name (pad));
    return;
  }

  sinkpad = gst_element_get_static_pad (filesink, "sink");
  if (gst_pad_link (pad, sinkpad) != GST_PAD_LINK_OK)
    GST_DEBUG ("CANNOT link srcpad_automuxer with sinkpad_fakesink\n");
  else
    GST_DEBUG ("LINK srcpad_automuxer with sinkpad_fakesink\n");

  gst_object_unref (sinkpad);
}

void
pad_removed (GstElement * element, GstPad * pad, gpointer data)
{
  /* Empty function. Used just for testing purposes about signal handling */
  GST_DEBUG ("Pad removed callback");
}

GST_START_TEST (videoraw)
{
  test_timeout = FALSE;
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GstPadTemplate *automuxer_sink_pad_template_audio;
  GstPadTemplate *automuxer_sink_pad_template_video;
  GstPad *automuxer_audiosink_pad, *automuxer_videosink_pad;
  GstPad *audio_src_pad, *video_src_pad;

  //GstElement *pipeline;

  pipeline0 = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *automuxerbin = gst_element_factory_make ("automuxerbin", NULL);
  GstElement *filesink = gst_element_factory_make ("filesink", NULL);

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline0));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline0);
  g_object_unref (bus);

  g_object_set (G_OBJECT (videotestsrc), "num-buffers", 250, NULL);
  g_object_set (G_OBJECT (audiotestsrc), "num-buffers", 440, NULL);
  g_object_set (G_OBJECT (filesink), "location", "test.avi", NULL);

  mark_point ();
  gst_bin_add_many (GST_BIN (pipeline0), audiotestsrc, videotestsrc,
      automuxerbin, filesink, NULL);

  g_signal_connect (automuxerbin, "pad-added", G_CALLBACK (pad_added),
      filesink);
  g_signal_connect (automuxerbin, "pad-removed", G_CALLBACK (pad_removed),
      filesink);

  /* Manually link the automuxer, which has "Request" pads */
  automuxer_sink_pad_template_audio =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (automuxerbin),
      "audio_sink_%u");
  automuxer_audiosink_pad =
      gst_element_request_pad (automuxerbin, automuxer_sink_pad_template_audio,
      NULL, NULL);
  g_print ("Obtained request pad %s for audio branch.\n",
      gst_pad_get_name (automuxer_audiosink_pad));
  audio_src_pad = gst_element_get_static_pad (audiotestsrc, "src");

  automuxer_sink_pad_template_video =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (automuxerbin),
      "video_sink_%u");
  automuxer_videosink_pad =
      gst_element_request_pad (automuxerbin, automuxer_sink_pad_template_video,
      NULL, NULL);
  g_print ("Obtained request pad %s for video branch.\n",
      gst_pad_get_name (automuxer_videosink_pad));
  video_src_pad = gst_element_get_static_pad (videotestsrc, "src");

  if (gst_pad_link (audio_src_pad, automuxer_audiosink_pad) != GST_PAD_LINK_OK
      || gst_pad_link (video_src_pad,
          automuxer_videosink_pad) != GST_PAD_LINK_OK) {
    g_print ("automuxer could not be linked.\n");
    g_main_loop_quit (loop);
    gst_object_unref (pipeline0);
  }
  gst_object_unref (video_src_pad);
  gst_object_unref (audio_src_pad);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline0),
      GST_DEBUG_GRAPH_SHOW_ALL, "before_entering_loop_videoraw");
  g_timeout_add (2000, (GSourceFunc) timeout, loop);

  GST_INFO ("Prueba: g_main_loop_run (loop);\n");

  gst_element_set_state (pipeline0, GST_STATE_PLAYING);

  mark_point ();
  g_main_loop_run (loop);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline0),
      GST_DEBUG_GRAPH_SHOW_ALL, "create_buffer_test_end_videoraw");

  gst_element_set_state (pipeline0, GST_STATE_NULL);

  /* Release the request pads from automuxerbin, and unref them */
  gst_element_release_request_pad (automuxerbin, automuxer_audiosink_pad);
  gst_element_release_request_pad (automuxerbin, automuxer_videosink_pad);
  gst_object_unref (automuxer_audiosink_pad);
  gst_object_unref (automuxer_videosink_pad);

  g_object_unref (pipeline0);
  g_main_loop_unref (loop);

}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
sdp_suite (void)
{
  Suite *s = suite_create ("automuxerbin");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, videoraw);

  return s;
}

GST_CHECK_MAIN (sdp);
