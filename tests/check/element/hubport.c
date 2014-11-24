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

#define HUB_AUDIO_SINK "hub_audio_sink"
#define HUB_VIDEO_SINK "hub_video_sink"

GST_START_TEST (connect_srcs)
{
  GstElement *hubport = gst_element_factory_make ("hubport", NULL);
  GstPad *sink, *src;

  src = gst_element_get_request_pad (hubport, "video_src_%u");
  fail_unless (src == NULL);
  sink = gst_element_get_request_pad (hubport, HUB_VIDEO_SINK);
  fail_unless (sink != NULL);
  fail_unless (g_strcmp0 (GST_OBJECT_NAME (sink), HUB_VIDEO_SINK) == 0);
  src = gst_element_get_request_pad (hubport, "video_src_%u");
  fail_unless (src != NULL);
  g_object_unref (src);
  g_object_unref (sink);

  src = gst_element_get_request_pad (hubport, "audio_src_%u");
  fail_unless (src == NULL);
  sink = gst_element_get_request_pad (hubport, HUB_AUDIO_SINK);
  fail_unless (sink != NULL);
  fail_unless (g_strcmp0 (GST_OBJECT_NAME (sink), HUB_AUDIO_SINK) == 0);
  src = gst_element_get_request_pad (hubport, "audio_src_%u");
  fail_unless (src != NULL);
  g_object_unref (src);
  g_object_unref (sink);

  g_object_unref (hubport);
}

GST_END_TEST
GST_START_TEST (connect_sinks)
{
  GstBin *pipe = (GstBin *) gst_pipeline_new ("connect_sinks");
  GstElement *hubport = gst_element_factory_make ("hubport", NULL);
  GstElement *videosrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *audiosrc = gst_element_factory_make ("audiotestsrc", NULL);
  GstElement *fakesink1 = gst_element_factory_make ("fakesink", NULL);
  GstElement *fakesink2 = gst_element_factory_make ("fakesink", NULL);

  gst_bin_add_many (pipe, hubport, videosrc, audiosrc, fakesink1, fakesink2,
      NULL);

  /* By default sink pads does not exists until hub port is internally connected */

  fail_if (gst_element_get_static_pad (hubport, "sink_video"));
  fail_if (gst_element_get_static_pad (hubport, "sink_audio"));

  /* Check if pads have been created now that internal pads have been linked */

  fail_unless (gst_element_link_pads (hubport, "hub_video_src", fakesink1,
          "sink"));
  fail_unless (gst_element_link_pads (hubport, "hub_audio_src", fakesink2,
          "sink"));

  fail_unless (gst_element_link_pads (videosrc, "src", hubport, "sink_video"));
  fail_unless (gst_element_link_pads (audiosrc, "src", hubport, "sink_audio"));

  g_object_unref (pipe);
}

GST_END_TEST
GST_START_TEST (create_element)
{
  GstElement *hubport;

  hubport = gst_element_factory_make ("hubport", NULL);

  fail_unless (hubport != NULL);

  g_object_unref (hubport);
}

GST_END_TEST static Suite *
hubport_suite (void)
{
  Suite *s = suite_create ("hubport");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_element);
  tcase_add_test (tc_chain, connect_sinks);
  tcase_add_test (tc_chain, connect_srcs);

  return s;
}

GST_CHECK_MAIN (hubport);
