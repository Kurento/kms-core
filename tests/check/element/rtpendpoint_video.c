/*
 * rtpendpoint_video.c - gst-kurento-plugins
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
#include <gst/sdp/gstsdpmessage.h>
#include <gst/gst.h>
#include <glib.h>

// TODO: create a generic bus_msg
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

typedef struct HandOffData
{
  GMainLoop *loop;
  GstStaticCaps expected_caps;
} HandOffData;

static void
fakesink_hand_off (GstElement * fakesink, GstBuffer * buf, GstPad * pad,
    gpointer data)
{
  HandOffData *hod = (HandOffData *) data;
  GstCaps *caps, *expected_caps;
  gboolean is_subset = FALSE;

  expected_caps = gst_static_caps_get (&hod->expected_caps);
  caps = gst_pad_get_current_caps (pad);
  is_subset = gst_caps_is_subset (caps, expected_caps);
  GST_DEBUG ("expected caps: %P, caps: %P, is subset: %d", expected_caps, caps,
      is_subset);
  gst_caps_unref (expected_caps);
  gst_caps_unref (caps);

  g_main_loop_quit (hod->loop);
  fail_unless (is_subset);
}

// FIXME: if agnosticbin is used, the test fails

static void
test_video_sendonly (const gchar * video_enc_name, GstStaticCaps expected_caps,
    const gchar * pattern_sdp_sendonly_str,
    const gchar * pattern_sdp_recvonly_str, gboolean play_after_negotiation)
{
  HandOffData *hod;
  GMainLoop *loop = g_main_loop_new (NULL, TRUE);
  GstSDPMessage *pattern_sdp, *offer, *answer;
  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *video_enc = gst_element_factory_make (video_enc_name, NULL);
  GstElement *rtpendpointsender =
      gst_element_factory_make ("rtpendpoint", NULL);
  GstElement *rtpendpointreceiver =
      gst_element_factory_make ("rtpendpoint", NULL);
  GstElement *outputfakesink = gst_element_factory_make ("fakesink", NULL);

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  fail_unless (gst_sdp_message_new (&pattern_sdp) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          pattern_sdp_sendonly_str, -1, pattern_sdp) == GST_SDP_OK);
  g_object_set (rtpendpointsender, "pattern-sdp", pattern_sdp, NULL);
  fail_unless (gst_sdp_message_free (pattern_sdp) == GST_SDP_OK);

  fail_unless (gst_sdp_message_new (&pattern_sdp) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          pattern_sdp_recvonly_str, -1, pattern_sdp) == GST_SDP_OK);
  g_object_set (rtpendpointreceiver, "pattern-sdp", pattern_sdp, NULL);
  fail_unless (gst_sdp_message_free (pattern_sdp) == GST_SDP_OK);

  hod = g_slice_new (HandOffData);
  hod->expected_caps = expected_caps;
  hod->loop = loop;

  g_object_set (G_OBJECT (outputfakesink), "signal-handoffs", TRUE, NULL);
  g_signal_connect (G_OBJECT (outputfakesink), "handoff",
      G_CALLBACK (fakesink_hand_off), hod);

  /* Add elements */
  gst_bin_add_many (GST_BIN (pipeline), videotestsrc, video_enc,
      rtpendpointsender, NULL);
  gst_element_link (videotestsrc, video_enc);
  gst_element_link_pads (video_enc, NULL, rtpendpointsender, "video_sink");

  gst_bin_add_many (GST_BIN (pipeline), rtpendpointreceiver, outputfakesink,
      NULL);
  gst_element_link_pads (rtpendpointreceiver, "video_src_%u", outputfakesink,
      "sink");

  if (!play_after_negotiation)
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* SDP negotiation */
  mark_point ();
  g_signal_emit_by_name (rtpendpointsender, "generate-offer", &offer);
  fail_unless (offer != NULL);

  mark_point ();
  g_signal_emit_by_name (rtpendpointreceiver, "process-offer", offer, &answer);
  fail_unless (answer != NULL);

  mark_point ();
  g_signal_emit_by_name (rtpendpointsender, "process-answer", answer);
  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);

  if (play_after_negotiation)
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "test_sendonly_before_entering_loop");

  mark_point ();
  g_main_loop_run (loop);
  mark_point ();

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "test_sendonly_end");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
  g_slice_free (HandOffData, hod);
}

/* VP8 tests */

static GstStaticCaps vp8_expected_caps = GST_STATIC_CAPS ("video/x-vp8");

static const gchar *pattern_sdp_vp8_sendonly_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=sendonly\r\n";

static const gchar *pattern_sdp_vp8_recvonly_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=recvonly\r\n";

static void
test_vp8_sendonly (gboolean play_after_negotiation)
{
  test_video_sendonly ("vp8enc", vp8_expected_caps,
      pattern_sdp_vp8_sendonly_str, pattern_sdp_vp8_recvonly_str,
      play_after_negotiation);
}

GST_START_TEST (test_vp8_sendonly_play_before_negotiation)
{
  test_vp8_sendonly (FALSE);
}

GST_END_TEST
GST_START_TEST (test_vp8_sendonly_play_after_negotiation)
{
  test_vp8_sendonly (TRUE);
}

GST_END_TEST
/* H264 tests */
static GstStaticCaps h264_expected_caps = GST_STATIC_CAPS ("video/x-h264");

static const gchar *pattern_sdp_h264_sendonly_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96\r\n" "a=rtpmap:96 H264/90000\r\n" "a=sendonly\r\n";

static const gchar *pattern_sdp_h264_recvonly_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96\r\n" "a=rtpmap:96 H264/90000\r\n" "a=recvonly\r\n";

static void
test_h264_sendonly (gboolean play_after_negotiation)
{
  test_video_sendonly ("x264enc", h264_expected_caps,
      pattern_sdp_h264_sendonly_str, pattern_sdp_h264_recvonly_str,
      play_after_negotiation);
}

GST_START_TEST (test_h264_sendonly_play_before_negotiation)
{
  test_h264_sendonly (FALSE);
}

GST_END_TEST
GST_START_TEST (test_h264_sendonly_play_after_negotiation)
{
  test_h264_sendonly (TRUE);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
rtpendpoint_video_test_suite (void)
{
  Suite *s = suite_create ("rtpendpoint_video");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_vp8_sendonly_play_before_negotiation);
  tcase_add_test (tc_chain, test_vp8_sendonly_play_after_negotiation);

  tcase_add_test (tc_chain, test_h264_sendonly_play_before_negotiation);
  tcase_add_test (tc_chain, test_h264_sendonly_play_after_negotiation);

  return s;
}

GST_CHECK_MAIN (rtpendpoint_video_test);
