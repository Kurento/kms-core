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

#define MIXER_AUDIO_SINK "mixer_audio_sink"
#define MIXER_VIDEO_SINK "mixer_video_sink"

static void
connect_sinks_pad_added (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstElement *fakesink;
  GstBin *pipe = GST_BIN (user_data);

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  if (!g_str_has_prefix (GST_OBJECT_NAME (pad), "mixer"))
    return;

  fakesink = gst_element_factory_make ("fakesink", GST_OBJECT_NAME (pad));

  gst_bin_add (pipe, fakesink);

  gst_element_link_pads (element, GST_OBJECT_NAME (pad), fakesink, "sink");
  GST_DEBUG_OBJECT (element, "Pad added: %" GST_PTR_FORMAT, pad);
}

static void
unlink_src_pad (GstElement * element, const gchar * pad_name)
{
  GstPad *pad = gst_element_get_static_pad (element, pad_name);
  GstPad *peer = gst_pad_get_peer (pad);

  gst_pad_unlink (pad, peer);

  g_object_unref (pad);
  g_object_unref (peer);
}

GST_START_TEST (connect_srcs)
{
  GstElement *mixerendpoint = gst_element_factory_make ("mixerendpoint", NULL);
  GstPad *sink, *src;

  src = gst_element_get_request_pad (mixerendpoint, "video_src_%u");
  fail_unless (src == NULL);
  sink = gst_element_get_request_pad (mixerendpoint, MIXER_VIDEO_SINK);
  fail_unless (sink != NULL);
  fail_unless (g_strcmp0 (GST_OBJECT_NAME (sink), MIXER_VIDEO_SINK) == 0);
  src = gst_element_get_request_pad (mixerendpoint, "video_src_%u");
  fail_unless (src != NULL);
  g_object_unref (src);
  g_object_unref (sink);

  src = gst_element_get_request_pad (mixerendpoint, "audio_src_%u");
  fail_unless (src == NULL);
  sink = gst_element_get_request_pad (mixerendpoint, MIXER_AUDIO_SINK);
  fail_unless (sink != NULL);
  fail_unless (g_strcmp0 (GST_OBJECT_NAME (sink), MIXER_AUDIO_SINK) == 0);
  src = gst_element_get_request_pad (mixerendpoint, "audio_src_%u");
  fail_unless (src != NULL);
  g_object_unref (src);
  g_object_unref (sink);

  g_object_unref (mixerendpoint);
}

GST_END_TEST
GST_START_TEST (connect_sinks)
{
  GstBin *pipe = (GstBin *) gst_pipeline_new ("connect_sinks");
  GstElement *mixerendpoint = gst_element_factory_make ("mixerendpoint", NULL);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiosrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *valve0, *valve1;
  gboolean drop;

  gst_bin_add_many (pipe, mixerendpoint, videosrc, audiosrc, NULL);

  g_signal_connect (mixerendpoint, "pad-added",
      G_CALLBACK (connect_sinks_pad_added), pipe);

  gst_element_link_pads (videosrc, "src", mixerendpoint, "video_sink");
  gst_element_link_pads (audiosrc, "src", mixerendpoint, "audio_sink");

  /* Check if valves have been opened because of fakesink link */

  valve0 = gst_bin_get_by_name (GST_BIN (mixerendpoint), "valve0");
  fail_unless (valve0 != NULL);
  GST_DEBUG ("Got valve: %" GST_PTR_FORMAT, valve0);
  drop = TRUE;
  g_object_get (G_OBJECT (valve0), "drop", &drop, NULL);
  fail_unless (drop == FALSE);
  GST_DEBUG ("Drop value: %d", drop);

  valve1 = gst_bin_get_by_name (GST_BIN (mixerendpoint), "valve1");
  fail_unless (valve1 != NULL);
  GST_DEBUG ("Got valve: %" GST_PTR_FORMAT, valve1);
  drop = TRUE;
  g_object_get (G_OBJECT (valve1), "drop", &drop, NULL);
  fail_unless (drop == FALSE);
  GST_DEBUG ("Drop value: %d", drop);

  /* Now check that valves are closed when mixer_src pads are unlinked */
  unlink_src_pad (mixerendpoint, "mixer_video_src");
  unlink_src_pad (mixerendpoint, "mixer_audio_src");

  drop = FALSE;
  g_object_get (G_OBJECT (valve0), "drop", &drop, NULL);
  fail_unless (drop == TRUE);
  GST_DEBUG_OBJECT (valve0, "Drop value: %d", drop);

  drop = FALSE;
  g_object_get (G_OBJECT (valve1), "drop", &drop, NULL);
  fail_unless (drop == TRUE);
  GST_DEBUG_OBJECT (valve0, "Drop value: %d", drop);

  g_object_unref (valve0);
  g_object_unref (valve1);
  g_object_unref (pipe);
}

GST_END_TEST
GST_START_TEST (create_element)
{
  GstElement *mixerendpoint;

  mixerendpoint = gst_element_factory_make ("mixerendpoint", NULL);

  fail_unless (mixerendpoint != NULL);

  g_object_unref (mixerendpoint);
}

GST_END_TEST static Suite *
mixerendpoint_suite (void)
{
  Suite *s = suite_create ("mixerendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_element);
  tcase_add_test (tc_chain, connect_sinks);
  tcase_add_test (tc_chain, connect_srcs);

  return s;
}

GST_CHECK_MAIN (mixerendpoint);
