/*
 * udpstream.c - gst-kurento-plugins
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

static const gchar *pattern_sdp_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96 97\r\n"
    "a=rtpmap:96 VP8/90000\r\n"
    "a=rtpmap:97 H263-1998/90000\r\n"
    "m=audio 0 RTP/AVP 98 99\r\n"
    "a=rtpmap:98 OPUS/48000/1\r\n" "a=rtpmap:99 AMR/8000/1\r\n";

GST_START_TEST (negotiation_offerer)
{
  GstSDPMessage *pattern_sdp;
  GstElement *udpstream = gst_element_factory_make ("udpstream", NULL);
  GstSDPMessage *offer = NULL;
  gchar *aux = NULL;

  fail_unless (gst_sdp_message_new (&pattern_sdp) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *) pattern_sdp_str,
          -1, pattern_sdp) == GST_SDP_OK);

  g_object_set (udpstream, "pattern-sdp", pattern_sdp, NULL);
  fail_unless (gst_sdp_message_free (pattern_sdp) == GST_SDP_OK);
  g_object_get (udpstream, "pattern-sdp", &pattern_sdp, NULL);
  fail_unless (pattern_sdp != NULL);
  fail_unless (gst_sdp_message_free (pattern_sdp) == GST_SDP_OK);

  g_signal_emit_by_name (udpstream, "generate-offer", &offer);

  fail_unless (offer != NULL);

  GST_DEBUG ("Offer:\n%s", (aux = gst_sdp_message_as_text (offer)));
  g_free (aux);

  gst_sdp_message_free (offer);
  g_object_unref (udpstream);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
sdp_suite (void)
{
  Suite *s = suite_create ("udpstream");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, negotiation_offerer);

  return s;
}

GST_CHECK_MAIN (sdp);
