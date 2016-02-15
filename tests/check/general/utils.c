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
#include "kmsutils.h"
#include "sdp_utils.h"

#include <gst/check/gstcheck.h>
#include <glib.h>

GST_START_TEST (check_urls)
{
  gchar *uri = "http://192.168.0.111:8080repository_servlet/video-upload";

  fail_if (kms_is_valid_uri (uri));

  uri = "http://192.168.0.111:8080/repository_servlet/video-upload";
  fail_if (!(kms_is_valid_uri (uri)));

  uri = "http://www.kurento.es/resource";
  fail_if (!(kms_is_valid_uri (uri)));

  uri = "http://localhost:8080/resource/res";
  fail_if (!(kms_is_valid_uri (uri)));

}

GST_END_TEST;

/* *INDENT-OFF* */
static const gchar *sdp_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 100 116 117 96\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtpmap:100 VP8/90000\r\n"
    "a=rtpmap:116 red/90000\r\n"
    "a=rtpmap:117 ulpfec/90000\r\n"
    "a=rtpmap:96 rtx/90000\r\n"
    "a=fmtp:96 apt=100\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=rtcp-fb:100 nack\r\n"
    "a=rtcp-fb:100 nack pli\r\n"
    "a=rtcp-fb:100 goog-remb\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:4 urn:3gpp:video-orientation\r\n"
    "a=setup:actpass\r\n"
    "a=mid:video-1733429841\r\n"
    "a=msid:nnnwYrPTpGmyoJX5GFHMVv42y1ZthbnCx26c 9203939c-25cf-4d60-82c2-d25b19350926\r\n"
    "a=sendrecv\r\n"
    "a=ice-ufrag:xHOGnBsKDPCmHB5t\r\n"
    "a=ice-pwd:qpnbhhoyeTrypBkX5F1u338T\r\n"
    "a=fingerprint:sha-256 58:E0:FE:56:6A:8C:5A:AD:71:5B:A0:52:47:27:60:66:27:53:EC:B6:F3:03:A8:4B:9B:30:28:62:29:49:C6:73\r\n"
    "a=ssrc:1733429841 cname:5YcASuDc3X86mu+d\r\n"
    "a=ssrc:1733429841 mslabel:nnnwYrPTpGmyoJX5GFHMVv42y1ZthbnCx26c\r\n"
    "a=ssrc:1733429841 label:9203939c-25cf-4d60-82c2-d25b19350926\r\n"
    "a=ssrc:2560713622 cname:5YcASuDc3X86mu+d\r\n"
    "a=ssrc:2560713622 mslabel:nnnwYrPTpGmyoJX5GFHMVv42y1ZthbnCx26c\r\n"
    "a=ssrc:2560713622 label:9203939c-25cf-4d60-82c2-d25b19350926\r\n"
    "a=ssrc-group:FID 2560713622 1733429841\r\n"
    "a=rtcp-mux\r\n";
/* *INDENT-ON* */

GST_START_TEST (check_sdp_utils_media_get_fid_ssrc)
{
  GstSDPMessage *message;
  const GstSDPMedia *media;
  guint ssrc;

  fail_unless (gst_sdp_message_new (&message) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          sdp_str, -1, message) == GST_SDP_OK);

  media = gst_sdp_message_get_media (message, 0);
  fail_if (media == NULL);

  ssrc = sdp_utils_media_get_fid_ssrc (media, 0);
  fail_if (ssrc != 2560713622);

  ssrc = sdp_utils_media_get_ssrc (media);
  fail_if (ssrc != 1733429841);
}

GST_END_TEST;

/* Suite initialization */
static Suite *
utils_suite (void)
{
  Suite *s = suite_create ("utils");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_urls);

  tcase_add_test (tc_chain, check_sdp_utils_media_get_fid_ssrc);

  return s;
}

GST_CHECK_MAIN (utils);
