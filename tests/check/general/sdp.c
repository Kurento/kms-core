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

#include "sdp_utils.h"
#include <gst/check/gstcheck.h>

static gchar *offer_sdp = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 3434 RTP/AVP 96 97 99\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=rtpmap:97 H263-1998/90000\r\n"
    "a=rtpmap:99 H263/90000\r\n"
    "a=sendrecv\r\n"
    "m=video 6565 RTP/AVP 98\r\n"
    "a=rtpmap:98 VP8/90000\r\n"
    "a=sendrecv\r\n" "m=audio 4545 RTP/AVP 14\r\n" "a=sendrecv\r\n"
    "m=audio 1010 TCP 14\r\n";

static gchar *answer_sdp = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 5656 RTP/AVP 96 34\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=sendrecv\r\n"
    "m=audio 6767 RTP/AVP 14\r\n" "a=rtpmap:14 MPA/90000\r\n" "a=sendonly\r\n";

static gchar *spected_offer = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 3434 RTP/AVP 96 99\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=rtpmap:99 H263/90000\r\n"
    "a=sendrecv\r\n"
    "m=video 0 RTP/AVP 98\r\n"
    "a=rtpmap:98 VP8/90000\r\n"
    "a=inactive\r\n" "m=audio 4545 RTP/AVP 14\r\n"
    "a=rtpmap:14 MPA/90000\r\n"
    "a=recvonly\r\n" "m=audio 0 TCP 14\r\n" "a=rtpmap:14 MPA/90000\r\n"
    "a=inactive\r\n";

static gchar *spected_answer = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 5656 RTP/AVP 96 99\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=rtpmap:99 H263/90000\r\n"
    "a=sendrecv\r\n"
    "m=video 0 RTP/AVP 98\r\n"
    "a=rtpmap:98 VP8/90000\r\n"
    "a=inactive\r\n" "m=audio 6767 RTP/AVP 14\r\n"
    "a=rtpmap:14 MPA/90000\r\n"
    "a=sendonly\r\n" "m=audio 0 TCP 14\r\n"
    "a=rtpmap:14 MPA/90000\r\n" "a=inactive\r\n";

GST_START_TEST (intersect)
{
  GstSDPMessage *offer, *answer;
  GstSDPMessage *offer_result, *answer_result;
  glong length;
  gchar *tmp = NULL;

  length = g_utf8_strlen (offer_sdp, -1);
  gst_sdp_message_new (&offer);
  gst_sdp_message_parse_buffer ((guint8 *) offer_sdp, length, offer);
  GST_DEBUG ("%s", offer_sdp);
  GST_DEBUG ("SDP offer: \n%s", tmp = gst_sdp_message_as_text (offer));
  if (tmp != NULL) {
    g_free (tmp);
    tmp = NULL;
  }

  length = g_utf8_strlen (answer_sdp, -1);
  gst_sdp_message_new (&answer);
  gst_sdp_message_parse_buffer ((guint8 *) answer_sdp, length, answer);
  GST_DEBUG ("SDP answer: \n%s", tmp = gst_sdp_message_as_text (answer));
  if (tmp != NULL) {
    g_free (tmp);
    tmp = NULL;
  }

  sdp_utils_intersect_sdp_messages (offer, answer, &offer_result,
      &answer_result);

  tmp = gst_sdp_message_as_text (offer_result);
  GST_DEBUG ("SDP offer result: \n%s", tmp);
  fail_if (g_strcmp0 (tmp, spected_offer) != 0);
  g_free (tmp);

  tmp = gst_sdp_message_as_text (answer_result);
  GST_DEBUG ("SDP asnwer result: \n%s", tmp);
  fail_if (g_strcmp0 (tmp, spected_answer) != 0);
  g_free (tmp);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  gst_sdp_message_free (offer_result);
  gst_sdp_message_free (answer_result);
}

GST_END_TEST
    static gchar *offer_str = "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=video 1 RTP/SAVPF 103 100\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=sendrecv\r\n" "a=rtpmap:103 H264/90000\r\n" "a=rtpmap:100 VP8/90000\r\n";

static gchar *answer_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 1 RTP/SAVPF 97\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtpmap:97 VP8/90000\r\n" "a=rtcp:1 IN IP4 0.0.0.0\r\n";

static gchar *expected_offer2 = "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=video 1 RTP/SAVPF 100\r\n"
    "c=IN IP4 0.0.0.0\r\n" "a=rtpmap:100 VP8/90000\r\n" "a=sendrecv\r\n";

GST_START_TEST (intersect_fail)
{
  GstSDPMessage *offer, *answer;
  GstSDPMessage *offer_result, *answer_result;
  gchar *tmp = NULL;

  gst_sdp_message_new (&offer);
  gst_sdp_message_parse_buffer ((guint8 *) offer_str, -1, offer);
  GST_DEBUG ("%s", offer_sdp);
  GST_DEBUG ("SDP offer: \n%s", tmp = gst_sdp_message_as_text (offer));
  if (tmp != NULL) {
    g_free (tmp);
    tmp = NULL;
  }

  gst_sdp_message_new (&answer);
  gst_sdp_message_parse_buffer ((guint8 *) answer_str, -1, answer);
  GST_DEBUG ("SDP answer: \n%s", tmp = gst_sdp_message_as_text (answer));
  if (tmp != NULL) {
    g_free (tmp);
    tmp = NULL;
  }

  sdp_utils_intersect_sdp_messages (offer, answer, &offer_result,
      &answer_result);

  tmp = gst_sdp_message_as_text (offer_result);
  GST_DEBUG ("SDP offer result: \n%s", tmp);
  fail_if (g_strcmp0 (tmp, expected_offer2) != 0);
  g_free (tmp);

  tmp = gst_sdp_message_as_text (answer_result);
  GST_DEBUG ("SDP asnwer result: \n%s", tmp);
//   fail_if (g_strcmp0 (tmp, expected_answer) != 0);
  g_free (tmp);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  gst_sdp_message_free (offer_result);
  gst_sdp_message_free (answer_result);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
sdp_suite (void)
{
  Suite *s = suite_create ("sdp");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, intersect);
  tcase_add_test (tc_chain, intersect_fail);

  return s;
}

GST_CHECK_MAIN (sdp);
