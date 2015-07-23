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
#include <glib.h>

#include "sdp_utils.h"
#include "kmssdpagent.h"
#include "kmsisdppayloadmanager.h"
#include "kmssdpmediahandler.h"
#include "kmssdppayloadmanager.h"
#include "kmssdpsctpmediahandler.h"
#include "kmssdprtpavpmediahandler.h"
#include "kmssdprtpavpfmediahandler.h"
#include "kmssdprtpsavpfmediahandler.h"

#define OFFERER_ADDR "222.222.222.222"
#define ANSWERER_ADDR "111.111.111.111"

typedef void (*CheckSdpNegotiationFunc) (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data);

static gchar *audio_codecs[] = {
  "PCMU/8000/1",
  "opus/48000/2",
  "AMR/8000/1"
};

static gchar *video_codecs[] = {
  "H263-1998/90000",
  "VP8/90000",
  "MP4V-ES/90000",
  "H264/90000"
};

static void
set_default_codecs (KmsSdpRtpAvpMediaHandler * handler, gchar ** audio_list,
    guint audio_len, gchar ** video_list, guint video_len)
{
  KmsSdpPayloadManager *ptmanager;
  GError *err = NULL;
  guint i;

  ptmanager = kms_sdp_payload_manager_new ();
  kms_sdp_rtp_avp_media_handler_use_payload_manager (handler,
      KMS_I_SDP_PAYLOAD_MANAGER (ptmanager), &err);

  for (i = 0; i < audio_len; i++) {
    fail_unless (kms_sdp_rtp_avp_media_handler_add_audio_codec (handler,
            audio_list[i], &err));
  }

  for (i = 0; i < video_len; i++) {
    fail_unless (kms_sdp_rtp_avp_media_handler_add_video_codec (handler,
            video_list[i], &err));
  }
}

static void
sdp_agent_create_offer (KmsSdpAgent * agent)
{
  GError *err = NULL;
  GstSDPMessage *offer;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  ctx = kms_sdp_agent_create_offer (agent, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  gst_sdp_message_free (offer);
  kms_sdp_message_context_destroy (ctx);
}

GST_START_TEST (sdp_agent_test_create_offer)
{
  KmsSdpAgent *agent;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  g_object_set (agent, "addr", OFFERER_ADDR, NULL);

  sdp_agent_create_offer (agent);

  g_object_set (agent, "use-ipv6", TRUE, "addr",
      "0:0:0:0:0:ffff:d4d4:d4d4", NULL);
  sdp_agent_create_offer (agent);

  g_object_unref (agent);
}

GST_END_TEST;

GST_START_TEST (sdp_agent_test_add_proto_handler)
{
  KmsSdpAgent *agent;
  KmsSdpMediaHandler *handler;
  gint id;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  handler =
      KMS_SDP_MEDIA_HANDLER (g_object_new (KMS_TYPE_SDP_MEDIA_HANDLER, NULL));
  fail_if (handler == NULL);

  /* Try to add an invalid handler */
  id = kms_sdp_agent_add_proto_handler (agent, "audio", handler);
  fail_if (id >= 0);
  g_object_unref (handler);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  id = kms_sdp_agent_add_proto_handler (agent, "application", handler);
  fail_if (id < 0);

  sdp_agent_create_offer (agent);

  g_object_unref (agent);
}

GST_END_TEST;

static const gchar *sdp_offer_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendonly\r\n"
    "m=video 9 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=sendonly\r\n";

GST_START_TEST (sdp_agent_test_rejected_negotiation)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;
  gchar *sdp_str = NULL;
  guint i, len;
  SdpMessageContext *ctx;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  id = kms_sdp_agent_add_proto_handler (answerer, "application", handler);
  fail_if (id < 0);

  fail_unless (gst_sdp_message_new (&offer) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          sdp_offer_str, -1, offer) == GST_SDP_OK);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (answer);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;

    media = gst_sdp_message_get_media (answer, i);
    fail_if (media == NULL);

    /* Media should have been rejected */
    fail_if (media->port != 0);
  }
  g_object_unref (answerer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

GST_END_TEST;

static const gchar *sdp_offer_no_common_media_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=audio 30000 RTP/AVP 0\r\n"
    "a=mid:1\r\n"
    "m=audio 30002 RTP/AVP 8\r\n"
    "a=mid:2\r\n" "m=audio 30004 RTP/AVP 3\r\n" "a=mid:3\r\n";

static void
test_sdp_pattern_offer (const gchar * sdp_patter, KmsSdpAgent * answerer,
    CheckSdpNegotiationFunc func, gpointer data)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  fail_unless (gst_sdp_message_new (&offer) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          sdp_patter, -1, offer) == GST_SDP_OK);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  if (func) {
    func (offer, answer, data);
  }

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

static void
check_unsupported_medias (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  guint i, len;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (answer);
  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;

    media = gst_sdp_message_get_media (answer, i);
    if (i < 1) {
      /* Only medias from 1 forward must be rejected */
      continue;
    }

    fail_if (media->port != 0);
  }
}

GST_START_TEST (sdp_agent_test_rejected_unsupported_media)
{
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  test_sdp_pattern_offer (sdp_offer_no_common_media_str, answerer,
      check_unsupported_medias, NULL);

  g_object_unref (answerer);
}

GST_END_TEST;

static const gchar *sdp_offer_sctp_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendonly\r\n"
    "m=video 9 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=sendonly\r\n"
    "m=application 9 DTLS/SCTP 5000 5001 5002\r\n"
    "a=setup:actpass\r\n"
    "a=mid:data\r\n"
    "a=sendonly\r\n"
    "a=sctpmap:5000 webrtc-datachannel 1024\r\n"
    "a=sctpmap:5001 bfcp 2\r\n"
    "a=sctpmap:5002 t38 1\r\n"
    "a=webrtc-datachannel:5000 stream=1;label=\"channel 1\";subprotocol=\"chat\"\r\n"
    "a=webrtc-datachannel:5000 stream=2;label=\"channel 2\";subprotocol=\"file transfer\";max_retr=3\r\n"
    "a=bfcp:5000 stream=2;label=\"channel 2\";subprotocol=\"file transfer\";max_retr=3\r\n";

GST_START_TEST (sdp_agent_test_sctp_negotiation)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;
  gchar *sdp_str = NULL;
  guint i, len;
  SdpMessageContext *ctx;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  id = kms_sdp_agent_add_proto_handler (answerer, "application", handler);
  fail_if (id < 0);

  fail_unless (gst_sdp_message_new (&offer) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          sdp_offer_sctp_str, -1, offer) == GST_SDP_OK);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (answer);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;

    media = gst_sdp_message_get_media (answer, i);
    fail_if (media == NULL);

    /* Media should have been rejected */
    if (g_strcmp0 (gst_sdp_media_get_media (media), "application") != 0) {
      fail_if (media->port != 0);
      continue;
    } else {
      fail_if (media->port == 0);
    }

    /* This negotiation should only have 5 attributes */
    fail_if (gst_sdp_media_attributes_len (media) != 4);
  }
  g_object_unref (answerer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

GST_END_TEST;

static gboolean
set_media_direction (GstSDPMedia * media, const gchar * direction)
{
  return gst_sdp_media_add_attribute (media, direction, "") == GST_SDP_OK;
}

static gboolean
expected_media_direction (const GstSDPMedia * media, const gchar * expected)
{
  guint i, attrs_len;

  attrs_len = gst_sdp_media_attributes_len (media);

  for (i = 0; i < attrs_len; i++) {
    const GstSDPAttribute *attr;

    attr = gst_sdp_media_get_attribute (media, i);

    if (g_ascii_strcasecmp (attr->key, expected) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

static void
negotiate_rtp_avp (const gchar * direction, const gchar * expected)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  const GstSDPMedia *media;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  g_object_set (offerer, "addr", OFFERER_ADDR, NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  g_object_set (answerer, "addr", ANSWERER_ADDR, NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  fail_unless (sdp_utils_for_each_media (offer,
          (GstSDPMediaFunc) set_media_direction, (gpointer) direction));

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  /* Check only audio media */
  media = gst_sdp_message_get_media (answer, 1);
  fail_if (media == NULL);

  fail_if (!expected_media_direction (media, expected));

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_rtp_avp_negotiation)
{
  negotiate_rtp_avp ("sendonly", "recvonly");
  negotiate_rtp_avp ("recvonly", "sendonly");
  negotiate_rtp_avp ("sendrecv", "sendrecv");
}

GST_END_TEST static void
check_all_is_negotiated (GstSDPMessage * offer, GstSDPMessage * answer)
{
  guint i, len;

  /* Same number of medias must be in answer */
  len = gst_sdp_message_medias_len (offer);

  fail_if (len != gst_sdp_message_medias_len (answer));

  for (i = 0; i < len; i++) {
    const GstSDPMedia *m_offer, *m_answer;

    m_offer = gst_sdp_message_get_media (offer, i);
    m_answer = gst_sdp_message_get_media (answer, i);

    /* Media should be active */
    fail_if (gst_sdp_media_get_port (m_answer) == 0);

    /* Media should have the same protocol */
    fail_if (g_strcmp0 (gst_sdp_media_get_proto (m_offer),
            gst_sdp_media_get_proto (m_answer)) != 0);
  }
}

GST_START_TEST (sdp_agent_test_rtp_avpf_negotiation)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  g_object_set (offerer, "addr", OFFERER_ADDR, NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  g_object_set (answerer, "addr", ANSWERER_ADDR, NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  check_all_is_negotiated (offer, answer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST
GST_START_TEST (sdp_agent_test_rtp_savpf_negotiation)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  g_object_set (offerer, "addr", OFFERER_ADDR, NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  g_object_set (answerer, "addr", ANSWERER_ADDR, NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  check_all_is_negotiated (offer, answer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST static gboolean
check_mid_attr (const GstSDPMedia * media, gpointer user_data)
{
  gboolean expected = *((gboolean *) user_data);
  guint i, len;

  len = gst_sdp_media_attributes_len (media);

  for (i = 0; i < len; i++) {
    const GstSDPAttribute *a;

    a = gst_sdp_media_get_attribute (media, i);

    fail_if (g_strcmp0 (a->key, "mid") == 0 && !expected);
  }

  return TRUE;
}

static void
test_bundle_group (gboolean expected_bundle)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint gid, hid;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  gid = kms_sdp_agent_crate_bundle_group (offerer);
  fail_if (gid < 0);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  hid = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (hid < 0);

  /* Add video to bundle group */
  fail_unless (kms_sdp_agent_add_handler_to_group (offerer, gid, hid));

  /* re-use handler for audio */
  g_object_ref (handler);
  hid = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (hid < 0);

  /* Add audio to bundle group */
  fail_unless (kms_sdp_agent_add_handler_to_group (offerer, gid, hid));

  if (expected_bundle) {
    gid = kms_sdp_agent_crate_bundle_group (answerer);
  }

  /* re-use handler for video in answerer */
  g_object_ref (handler);
  hid = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (hid < 0);

  if (expected_bundle) {
    fail_unless (kms_sdp_agent_add_handler_to_group (answerer, gid, hid));
  }

  g_object_ref (handler);
  hid = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (hid < 0);

  if (expected_bundle) {
    fail_unless (kms_sdp_agent_add_handler_to_group (answerer, gid, hid));
  }

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  if (!expected_bundle) {
    fail_if (gst_sdp_message_get_attribute_val (answer, "group") != NULL);
  } else {
    fail_if (gst_sdp_message_get_attribute_val (answer, "group") == NULL);
  }

  sdp_utils_for_each_media (answer, check_mid_attr, &expected_bundle);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

static const gchar *sdp_no_bundle_group_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "a=group:LS video0 audio0\r\n"
    "m=video 1 RTP/SAVPF 96 97 100 101\r\n"
    "a=rtpmap:96 H263-1998/90000\r\n"
    "a=rtpmap:97 VP8/90000\r\n"
    "a=rtpmap:100 MP4V-ES/90000\r\n"
    "a=rtpmap:101 H264/90000\r\n"
    "a=rtcp-fb:97 nack\r\n"
    "a=rtcp-fb:97 nack pli\r\n"
    "a=rtcp-fb:97 ccm fir\r\n"
    "a=rtcp-fb:97 goog-remb\r\n"
    "a=rtcp-fb:101 nack\r\n"
    "a=rtcp-fb:101 nack pli\r\n"
    "a=rtcp-fb:101 ccm fir\r\n"
    "a=mid:video0\r\n"
    "m=audio 1 RTP/SAVPF 98 99 0\r\n"
    "a=rtpmap:98 OPUS/48000/2\r\n"
    "a=rtpmap:99 AMR/8000/1\r\n" "a=mid:audio0\r\n";

static const gchar *sdp_bundle_group_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 1 2 3\r\n"
    "m=audio 30000 RTP/SAVPF 0\r\n"
    "a=mid:1\r\n"
    "m=audio 30002 RTP/SAVPF 8\r\n"
    "a=mid:2\r\n" "m=audio 30004 RTP/SAVPF 3\r\n" "a=mid:3\r\n";

static void
check_no_group_attr (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  /* Only BUNDLE group is supported. Fail if any group */
  /* attribute is found in the answer */
  fail_unless (gst_sdp_message_get_attribute_val (answer, "group") == NULL);
}

static void
check_fmt_group_attr (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  const gchar *val;
  gchar **mids;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  val = gst_sdp_message_get_attribute_val (answer, "group");

  fail_if (val == NULL);
  mids = g_strsplit (val, " ", 0);

  /* only 1 media is supported for this group */
  fail_if (g_strv_length (mids) > 2);

  fail_if (g_strcmp0 (mids[0], "BUNDLE") != 0);

  /* Only mid 1 is supported */
  fail_if (g_strcmp0 (mids[1], "1") != 0);

  g_strfreev (mids);
}

static void
test_group_with_pattern (const gchar * sdp_pattern,
    CheckSdpNegotiationFunc func, gpointer data)
{
  KmsSdpMediaHandler *handler;
  KmsSdpAgent *answerer;
  gint gid, hid;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  gid = kms_sdp_agent_crate_bundle_group (answerer);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  hid = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (hid < 0);

  fail_unless (kms_sdp_agent_add_handler_to_group (answerer, gid, hid));

  /* re-use handler for audio */
  g_object_ref (handler);
  hid = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (hid < 0);

  fail_unless (kms_sdp_agent_add_handler_to_group (answerer, gid, hid));

  test_sdp_pattern_offer (sdp_pattern, answerer, func, NULL);

  g_object_unref (answerer);
}

static void
check_valid_bundle_answer (GstSDPMessage * answer)
{
  const gchar *val;
  gchar **mids;

  val = gst_sdp_message_get_attribute_val (answer, "group");

  fail_if (val == NULL);
  mids = g_strsplit (val, " ", 0);

  /* BUNDLE group must be empty */
  fail_if (g_strv_length (mids) > 1);

  fail_if (g_strcmp0 (mids[0], "BUNDLE") != 0);

  g_strfreev (mids);
}

static void
sdp_agent_test_bundle_group_without_answerer_handlers ()
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id, gid;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);
  gid = kms_sdp_agent_crate_bundle_group (offerer);
  fail_if (gid < 0);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);
  gid = kms_sdp_agent_crate_bundle_group (answerer);
  fail_if (gid < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);
  fail_unless (kms_sdp_agent_add_handler_to_group (offerer, gid, id));

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);
  fail_unless (kms_sdp_agent_add_handler_to_group (offerer, gid, id));

  /* Not adding any handler to answerer */

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);

  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  check_valid_bundle_answer (answer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_bundle_group)
{
  test_bundle_group (FALSE);
  test_bundle_group (TRUE);
  test_group_with_pattern (sdp_no_bundle_group_str, check_no_group_attr, NULL);
  test_group_with_pattern (sdp_bundle_group_str, check_fmt_group_attr, NULL);
  sdp_agent_test_bundle_group_without_answerer_handlers ();
}

GST_END_TEST static gboolean
is_rtcp_fb_in_media (const GstSDPMessage * msg, const gchar * type)
{
  guint i, len;

  len = gst_sdp_message_medias_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;
    guint j;

    media = gst_sdp_message_get_media (msg, i);

    for (j = 0;; j++) {
      const gchar *val;
      gchar **opts;

      val = gst_sdp_media_get_attribute_val_n (media, "rtcp-fb", j);

      if (val == NULL) {
        return FALSE;
      }

      opts = g_strsplit (val, " ", 0);

      if (g_strcmp0 (opts[1], type) == 0) {
        g_strfreev (opts);
        return TRUE;
      }

      g_strfreev (opts);
    }
  }

  return FALSE;
}

static void
fb_messages_disable_offer_prop (const gchar * prop)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  g_object_set (handler, prop, FALSE, NULL);

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  fail_if (is_rtcp_fb_in_media (offer, prop));

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  fail_if (is_rtcp_fb_in_media (answer, prop));

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

static void
fb_messages_disable_answer_prop (const gchar * prop)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));
  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  g_object_set (handler, prop, FALSE, NULL);

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  fail_if (is_rtcp_fb_in_media (answer, prop));

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_fb_messages)
{
  fb_messages_disable_offer_prop ("nack");
  fb_messages_disable_answer_prop ("nack");
  fb_messages_disable_offer_prop ("goog-remb");
  fb_messages_disable_answer_prop ("goog-remb");
}

GST_END_TEST static void
test_handler_offer (KmsSdpAgent * offerer, KmsSdpAgent * answerer,
    CheckSdpNegotiationFunc func, gpointer data)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  if (func) {
    func (offer, answer, data);
  }

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

static gboolean
is_rtcp_mux_in_media (const GstSDPMessage * msg)
{
  guint i, len;

  len = gst_sdp_message_medias_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (msg, i);

    if (gst_sdp_media_get_attribute_val (media, "rtcp-mux") != NULL) {
      return TRUE;
    }
  }

  return FALSE;
}

static void
check_rtcp_mux_enabled (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  fail_unless (is_rtcp_mux_in_media (offer));
  fail_unless (is_rtcp_mux_in_media (answer));
}

static void
test_rtcp_mux_offer_enabled ()
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  test_handler_offer (offerer, answerer, check_rtcp_mux_enabled, NULL);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

static void
check_rtcp_mux_offer_disabled (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  fail_if (is_rtcp_mux_in_media (offer));
  fail_if (is_rtcp_mux_in_media (answer));
}

static void
check_rtcp_mux_answer_disabled (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  fail_unless (is_rtcp_mux_in_media (offer));
  fail_if (is_rtcp_mux_in_media (answer));
}

static void
test_rtcp_mux_offer_disabled ()
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  /* Offerer can not manage rtcp-mux */
  g_object_set (handler, "rtcp-mux", FALSE, NULL);

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  test_handler_offer (offerer, answerer, check_rtcp_mux_offer_disabled, NULL);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

static void
test_rtcp_mux_answer_disabled ()
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  /* Answerer can not manage rtcp-mux */
  g_object_set (handler, "rtcp-mux", FALSE, NULL);

  test_handler_offer (offerer, answerer, check_rtcp_mux_answer_disabled, NULL);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_rtcp_mux)
{
  test_rtcp_mux_offer_enabled ();
  test_rtcp_mux_offer_disabled ();
  test_rtcp_mux_answer_disabled ();
}

GST_END_TEST static void
check_multi_m_lines (const GstSDPMessage * offer, const GstSDPMessage * answer,
    gpointer data)
{
  guint i, len;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (offer);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *offer_m, *answer_m;

    offer_m = gst_sdp_message_get_media (offer, i);
    answer_m = gst_sdp_message_get_media (answer, i);

    fail_unless (g_strcmp0 (gst_sdp_media_get_media (offer_m),
            gst_sdp_media_get_media (answer_m)) == 0);

    fail_unless (g_strcmp0 (gst_sdp_media_get_proto (offer_m),
            gst_sdp_media_get_proto (answer_m)) == 0);
  }
}

GST_START_TEST (sdp_agent_test_multi_m_lines)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  /* First video entry */
  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* Re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  /* Re-use handler for video again */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* Re-use handler for audio again */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  /* Re-use handler for video in answers */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  /* Re-use handler for audio in answers */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  test_handler_offer (offerer, answerer, check_multi_m_lines, NULL);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST
    static const gchar *sdp_offer_unknown_attrs_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=audio 30000 RTP/AVP 0\r\n"
    "a=mid:1\r\n"
    "a=audiotestattr1:1\r\n"
    "a=audiotestattr2:1\r\n"
    "m=video 30002 RTP/AVP 8\r\n"
    "a=mid:2\r\n" "m=audio 30004 RTP/AVP 3\r\n" "a=mid:3\r\n"
    "a=videotestattr1:1\r\n";

static gboolean
check_media_attrs (const GstSDPMedia * media, gpointer user_data)
{
  if (g_strcmp0 (gst_sdp_media_get_media (media), "audio") == 0) {
    fail_if (gst_sdp_media_get_attribute_val (media, "audiotestattr1"));
    fail_if (gst_sdp_media_get_attribute_val (media, "audiotestattr2"));
  } else if (g_strcmp0 (gst_sdp_media_get_media (media), "video") == 0) {
    fail_if (gst_sdp_media_get_attribute_val (media, "videotestattr1"));
  } else {
    fail ("Media not included in offer");
  }

  return TRUE;
}

static void
check_unsupported_attrs (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  sdp_utils_for_each_media (answer, check_media_attrs, NULL);
}

GST_START_TEST (sdp_agent_test_filter_unknown_attr)
{
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  test_sdp_pattern_offer (sdp_offer_unknown_attrs_str, answerer,
      check_unsupported_attrs, NULL);

  g_object_unref (answerer);
}

GST_END_TEST
    static const gchar *sdp_offer_supported_attrs_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=Kurento Media Server\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=video 1 RTP/AVPF 96 97 100 101\r\n"
    "a=rtpmap:96 H265/90000\r\n"
    "a=rtpmap:97 VP8/90000\r\n"
    "a=rtpmap:100 MP4V-ES/90000\r\n"
    "a=rtpmap:101 H264/90000\r\n"
    "a=rtcp-fb:97 nack\r\n"
    "a=rtcp-fb:97 nack pli\r\n"
    "a=rtcp-fb:97 goog-remb\r\n"
    "a=rtcp-fb:97 ccm fir\r\n"
    "a=rtcp-fb:101 nack\r\n"
    "a=rtcp-fb:101 nack pli\r\n"
    "a=rtcp-fb:101 ccm fir\r\n"
    "a=fmtp:96 minptime=10; useinbandfec=1\r\n"
    "a=fmtp:97 minptime=10; useinbandfec=1\r\n"
    "a=fmtp:111 minptime=10; useinbandfec=1\r\n"
    "a=rtcp-mux\r\n"
    "a=quality:10\r\n" "a=maxptime:60\r\n" "a=setup:actpass\r\n";

static gboolean
check_supported_media_attrs (const GstSDPMedia * media, gpointer user_data)
{
  guint i;

  for (i = 0;; i++) {
    const gchar *val;

    val = gst_sdp_media_get_attribute_val_n (media, "fmtp", i);
    if (val == NULL) {
      /* no more fmtp attributes */
      break;
    }

    /* Only fmtp:97 must be in answer */
    fail_unless (g_str_has_prefix (val, "97"));
  }

  /* Only one fmtp lines should have been processed */
  fail_unless (i == 1);

  fail_if (gst_sdp_media_get_attribute_val (media, "quality") == NULL);
  fail_if (gst_sdp_media_get_attribute_val (media, "maxptime") == NULL);

  return TRUE;
}

static void
check_supported_attrs (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  sdp_utils_for_each_media (answer, check_supported_media_attrs, NULL);
}

GST_START_TEST (sdp_agent_test_supported_attrs)
{
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  test_sdp_pattern_offer (sdp_offer_supported_attrs_str, answerer,
      check_supported_attrs, NULL);

  g_object_unref (answerer);
}

GST_END_TEST typedef struct _BandwitdthData
{
  gboolean offered;
  guint offer;
  gboolean answered;
  guint answer;
} BandwitdthData;

static void
check_bandwidth_medias_attrs (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  const GstSDPBandwidth *offer_bw, *answer_bw;
  const GstSDPMedia *offered, *answered;
  BandwitdthData *bw = (BandwitdthData *) data;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  offered = gst_sdp_message_get_media (offer, 0);
  fail_if (offered == NULL);
  offer_bw = gst_sdp_media_get_bandwidth (offered, 0);

  if (bw->offered) {
    fail_if (offer_bw == NULL);
    fail_unless (offer_bw->bandwidth == bw->offer);
  } else {
    fail_if (offer_bw != NULL);
  }

  answered = gst_sdp_message_get_media (answer, 0);
  fail_if (answered == NULL);
  answer_bw = gst_sdp_media_get_bandwidth (answered, 0);

  if (bw->answered) {
    fail_if (answer_bw == NULL);
    fail_unless (answer_bw->bandwidth == bw->answer);
  } else {
    fail_if (answer_bw != NULL);
  }

}

static void
test_agents_bandwidth (gboolean offer, guint offered, gboolean answer,
    guint answered)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler1, *handler2;
  BandwitdthData data;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  handler1 = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler1 == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler1), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler1);
  fail_if (id < 0);

  if (offer) {
    kms_sdp_media_handler_add_bandwidth (handler1, "AS", offered);
  }

  handler2 = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler2 == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler2), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  if (answer) {
    kms_sdp_media_handler_add_bandwidth (handler2, "AS", answered);
  }

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler2);
  fail_if (id < 0);

  data.offered = offer;
  data.answered = answer;
  data.offer = offered;
  data.answer = answered;

  test_handler_offer (offerer, answerer, check_bandwidth_medias_attrs, &data);

  g_object_unref (answerer);
  g_object_unref (offerer);
}

GST_START_TEST (sdp_agent_test_bandwidtth_attrs)
{
  test_agents_bandwidth (TRUE, 120, TRUE, 15);
  test_agents_bandwidth (FALSE, 0, TRUE, 16);
  test_agents_bandwidth (TRUE, 16, FALSE, 0);
  test_agents_bandwidth (FALSE, 0, FALSE, 0);
}

GST_END_TEST;

static void
check_extmap_attrs_add_twice ()
{
  KmsSdpRtpAvpMediaHandler *handler;
  GError *err = NULL;
  gboolean ret;

  handler = kms_sdp_rtp_avp_media_handler_new ();
  fail_if (handler == NULL);

  ret = kms_sdp_rtp_avp_media_handler_add_extmap (handler, 1, "URI-A", &err);
  fail_if (ret == FALSE);
  fail_if (err != NULL);

  ret = kms_sdp_rtp_avp_media_handler_add_extmap (handler, 2, "URI-B", &err);
  fail_if (ret == FALSE);
  fail_if (err != NULL);

  ret = kms_sdp_rtp_avp_media_handler_add_extmap (handler, 1, "URI-A", &err);
  fail_if (ret == TRUE);
  fail_if (err == NULL);
  g_error_free (err);

  g_object_unref (handler);
}

static void
check_extmap_attrs_into_offer ()
{
  KmsSdpAgent *agent;
  KmsSdpMediaHandler *handler1;
  gchar *sdp_str = NULL;
  GstSDPMessage *offer;
  const GstSDPMedia *media;
  SdpMessageContext *ctx;
  const gchar *extmap;
  GError *err = NULL;
  gint id;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  handler1 = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler1 == NULL);

  kms_sdp_rtp_avp_media_handler_add_extmap (KMS_SDP_RTP_AVP_MEDIA_HANDLER
      (handler1), 1, "URI-A", &err);
  fail_if (err != NULL);

  id = kms_sdp_agent_add_proto_handler (agent, "video", handler1);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (agent, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  kms_sdp_message_context_destroy (ctx);
  fail_if (err != NULL);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  media = gst_sdp_message_get_media (offer, 0);
  extmap = gst_sdp_media_get_attribute_val (media, "extmap");
  fail_if (g_strcmp0 (extmap, "1 URI-A") != 0);

  gst_sdp_message_free (offer);
  g_object_unref (agent);
}

static void
check_extmap_attrs_negotiation ()
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler1, *handler2;
  gchar *sdp_str = NULL;
  GstSDPMessage *offer, *answer;
  const GstSDPMedia *media;
  SdpMessageContext *ctx;
  const gchar *extmap;
  GError *err = NULL;
  gint id;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  handler1 = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler1 == NULL);

  kms_sdp_rtp_avp_media_handler_add_extmap (KMS_SDP_RTP_AVP_MEDIA_HANDLER
      (handler1), 1, "URI-A", &err);
  fail_if (err != NULL);

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler1);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  kms_sdp_message_context_destroy (ctx);
  fail_if (err != NULL);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler2 = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler2 == NULL);

  kms_sdp_rtp_avp_media_handler_add_extmap (KMS_SDP_RTP_AVP_MEDIA_HANDLER
      (handler2), 2, "URI-A", &err);
  fail_if (err != NULL);

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler2);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  gst_sdp_message_free (offer);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  kms_sdp_message_context_destroy (ctx);
  fail_if (err != NULL);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  media = gst_sdp_message_get_media (answer, 0);
  extmap = gst_sdp_media_get_attribute_val (media, "extmap");
  fail_if (g_strcmp0 (extmap, "1 URI-A") != 0);

  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_extmap_attrs)
{
  check_extmap_attrs_add_twice ();
  check_extmap_attrs_into_offer ();
  check_extmap_attrs_negotiation ();
}

GST_END_TEST;

static void
test_sdp_dynamic_pts (KmsSdpRtpAvpMediaHandler * handler)
{
  KmsSdpPayloadManager *ptmanager;
  GError *err = NULL;

  /* Try to assign a dynamic pt to an static codec. Expected to fail */
  fail_if (kms_sdp_rtp_avp_media_handler_add_video_codec (handler,
          "H263-1998/90000", &err));
  GST_DEBUG ("Expected error: %s", err->message);
  g_clear_error (&err);

  ptmanager = kms_sdp_payload_manager_new ();
  kms_sdp_rtp_avp_media_handler_use_payload_manager (handler,
      KMS_I_SDP_PAYLOAD_MANAGER (ptmanager), &err);

  fail_unless (kms_sdp_rtp_avp_media_handler_add_video_codec (handler,
          "VP8/90000", &err));
  fail_unless (kms_sdp_rtp_avp_media_handler_add_video_codec (handler,
          "MP4V-ES/90000", &err));

  /* Try to add and already added codec. Expected to fail */
  fail_if (kms_sdp_rtp_avp_media_handler_add_video_codec (handler,
          "VP8/90000", &err));
  GST_DEBUG ("Expected error: %s", err->message);
  g_clear_error (&err);
}

GST_START_TEST (sdp_agent_test_dynamic_pts)
{
  KmsSdpMediaHandler *handler;
  KmsSdpAgent *agent;
  gint id;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  id = kms_sdp_agent_add_proto_handler (agent, "audio", handler);
  fail_if (id < 0);

  test_sdp_dynamic_pts (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler));

  g_object_unref (agent);
}

GST_END_TEST;

static const gchar *sdp_offer_str1 = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendonly\r\n";

static const gchar *sdp_offer_str2 = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000/1\r\n" "a=sendonly\r\n";

static const gchar *sdp_offer_str3 = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n" "m=audio 9 RTP/AVP 0\r\n" "a=sendonly\r\n";

static void
check_pcmu_without_number_of_channels (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  const GstSDPMedia *media;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  fail_if (gst_sdp_message_medias_len (answer) != 1);

  media = gst_sdp_message_get_media (answer, 0);

  fail_if (gst_sdp_media_get_port (media) == 0);
  fail_if (gst_sdp_media_formats_len (media) != 1);

  fail_unless (g_strcmp0 (gst_sdp_media_get_format (media, 0), "0") == 0);
}

static void
check_optional_number_of_channels (const gchar * offer, const gchar * codec)
{
  GError *err = NULL;
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  fail_unless (kms_sdp_rtp_avp_media_handler_add_audio_codec
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), codec, &err));

  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  test_sdp_pattern_offer (offer, answerer,
      check_pcmu_without_number_of_channels, NULL);

  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_optional_enc_parameters)
{
  check_optional_number_of_channels (sdp_offer_str1, "PCMU/8000/1");
  check_optional_number_of_channels (sdp_offer_str1, "PCMU/8000");
  check_optional_number_of_channels (sdp_offer_str2, "PCMU/8000/1");
  check_optional_number_of_channels (sdp_offer_str2, "PCMU/8000");
  check_optional_number_of_channels (sdp_offer_str3, "PCMU/8000/1");
  check_optional_number_of_channels (sdp_offer_str3, "PCMU/8000");
}

GST_END_TEST;

static gchar *offer_sdp_check_1 = "v=0\r\n"
    "o=- 123456 0 IN IP4 127.0.0.1\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 3434 RTP/AVP 96 97 99\r\n"
    "a=rtpmap:96 MP4V-ES/90000\r\n"
    "a=rtpmap:97 H263-1998/90000\r\n"
    "a=rtpmap:99 H264/90000\r\n"
    "a=sendrecv\r\n"
    "m=video 6565 RTP/AVP 98\r\n"
    "a=rtpmap:98 VP8/90000\r\n"
    "a=sendrecv\r\n" "m=audio 4545 RTP/AVP 14\r\n" "a=sendrecv\r\n"
    "m=audio 1010 TCP 14\r\n";

static void
intersection_check_1 (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  const GstSDPMedia *media;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  /***************************/
  /* Check first media entry */
  /***************************/
  media = gst_sdp_message_get_media (answer, 0);
  fail_unless (media != NULL);

  /* Video must be supported */
  fail_if (gst_sdp_media_get_port (media) == 0);

  /* Only format 96 and 99 are supported */
  fail_unless (gst_sdp_media_formats_len (media) != 1);
  fail_unless (g_strcmp0 (gst_sdp_media_get_format (media, 0), "96") == 0);
  fail_unless (g_strcmp0 (gst_sdp_media_get_format (media, 1), "99") == 0);

  /* Check that rtpmap attributes */
  fail_if (sdp_utils_sdp_media_get_rtpmap (media, "96") == NULL);
  fail_if (sdp_utils_sdp_media_get_rtpmap (media, "97") != NULL);
  fail_if (sdp_utils_sdp_media_get_rtpmap (media, "99") == NULL);

  /****************************/
  /* Check second media entry */
  /****************************/
  media = gst_sdp_message_get_media (answer, 1);
  fail_unless (media != NULL);

  /* Video should not be supported */
  fail_unless (gst_sdp_media_get_port (media) == 0);
  fail_if (sdp_utils_sdp_media_get_rtpmap (media, "98") != NULL);

  /***************************/
  /* Check third media entry */
  /***************************/
  media = gst_sdp_message_get_media (answer, 2);
  fail_unless (media != NULL);

  /* Audio should be supported */
  fail_if (gst_sdp_media_get_port (media) == 0);

  /* Only format 14 is supported */
  fail_unless (gst_sdp_media_formats_len (media) != 0);
  fail_unless (g_strcmp0 (gst_sdp_media_get_format (media, 0), "14") == 0);

  /****************************/
  /* Check fourth media entry */
  /****************************/
  media = gst_sdp_message_get_media (answer, 3);
  fail_unless (media != NULL);

  /* Audio should not be supported */
  fail_unless (gst_sdp_media_get_port (media) == 0);
}

static void
regression_test_1 ()
{
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  gchar *audio_codecs[] = {
    "MPA/90000"
  };

  gchar *video_codecs[] = {
    "MP4V-ES/90000",
    "H264/90000"
  };

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  /* Create video handler */
  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), NULL, 0,
      video_codecs, 2);

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  /* Create handler for audio */
  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs, 1,
      NULL, 0);

  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  /* Check response */
  test_sdp_pattern_offer (offer_sdp_check_1, answerer,
      intersection_check_1, NULL);

  g_object_unref (answerer);
}

static gchar *offer_sdp_check_2 = "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=video 1 RTP/SAVPF 103 100\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=sendrecv\r\n" "a=rtpmap:103 H264/90000\r\n" "a=rtpmap:100 VP8/90000\r\n";

static void
intersection_check_2 (const GstSDPMessage * offer,
    const GstSDPMessage * answer, gpointer data)
{
  const GstSDPMedia *media;

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  media = gst_sdp_message_get_media (answer, 0);
  fail_unless (media != NULL);

  /* Video must be supported */
  fail_if (gst_sdp_media_get_port (media) == 0);

  /* Only format 100 is supported */
  fail_unless (gst_sdp_media_formats_len (media) != 0);
  fail_unless (g_strcmp0 (gst_sdp_media_get_format (media, 0), "100") == 0);

  /* Check that rtpmap attributes are present */
  fail_if (sdp_utils_sdp_media_get_rtpmap (media, "100") == NULL);
}

static void
regression_test_2 ()
{
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gint id;

  gchar *video_codecs[] = {
    "VP8/90000"
  };

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  /* Create video handler */
  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), NULL, 0,
      video_codecs, 1);

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  /* Check response */
  test_sdp_pattern_offer (offer_sdp_check_2, answerer,
      intersection_check_2, NULL);

  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_regression_tests)
{
  regression_test_1 ();
  regression_test_2 ();
}

GST_END_TEST;

GST_START_TEST (sdp_agent_avp_avpf_negotiation)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  g_object_set (offerer, "addr", OFFERER_ADDR, NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  g_object_set (answerer, "addr", ANSWERER_ADDR, NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  check_all_is_negotiated (offer, answer);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST static void
check_payloads (GstSDPMessage * answer, gint fec_payload, gint red_payload)
{
  const GstSDPMedia *media = NULL;
  const gchar *attr;
  gchar *payload;
  guint i, l;

  l = gst_sdp_message_medias_len (answer);

  fail_if (l == 0);

  for (i = 0; i < l; i++) {
    media = gst_sdp_message_get_media (answer, i);

    if (g_strcmp0 (gst_sdp_media_get_media (media), "video") == 0) {
      break;
    }
  }

  fail_if (media == NULL ||
      g_strcmp0 (gst_sdp_media_get_media (media), "video") != 0);

  /* Check payloads */
  payload = g_strdup_printf ("%d", fec_payload);
  attr = sdp_utils_get_attr_map_value (media, "rtpmap", payload);
  g_free (payload);

  fail_if (attr == NULL);

  payload = g_strdup_printf ("%d", red_payload);
  attr = sdp_utils_get_attr_map_value (media, "rtpmap", payload);
  g_free (payload);

  fail_if (attr == NULL);

  payload = g_strdup_printf ("%d", red_payload);
  attr = sdp_utils_get_attr_map_value (media, "fmtp", payload);
  g_free (payload);

  fail_if (attr == NULL);
}

GST_START_TEST (sdp_agent_avp_generic_payload_negotiation)
{
  KmsSdpPayloadManager *ptmanager;
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gint id, fec_payload, red_payload;
  gchar *sdp_str = NULL;
  SdpMessageContext *ctx;

  ptmanager = kms_sdp_payload_manager_new ();

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  g_object_set (offerer, "addr", OFFERER_ADDR, NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  g_object_set (answerer, "addr", ANSWERER_ADDR, NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  kms_sdp_rtp_avp_media_handler_use_payload_manager
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler),
      KMS_I_SDP_PAYLOAD_MANAGER (ptmanager), &err);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  fec_payload = kms_sdp_rtp_avp_media_handler_add_generic_video_payload
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), "ulpfec/90000", &err);
  fail_if (err != NULL);

  red_payload = kms_sdp_rtp_avp_media_handler_add_generic_video_payload
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), "red/8000", &err);
  fail_if (err != NULL);

  kms_sdp_rtp_avp_media_handler_add_fmtp (KMS_SDP_RTP_AVP_MEDIA_HANDLER
      (handler), red_payload, "0/5/100", &err);
  fail_if (err != NULL);

  id = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_if (id < 0);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  ptmanager = kms_sdp_payload_manager_new ();

  kms_sdp_rtp_avp_media_handler_use_payload_manager
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler),
      KMS_I_SDP_PAYLOAD_MANAGER (ptmanager), &err);

  set_default_codecs (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), audio_codecs,
      G_N_ELEMENTS (audio_codecs), video_codecs, G_N_ELEMENTS (video_codecs));

  kms_sdp_rtp_avp_media_handler_add_generic_video_payload
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), "ulpfec/90000", &err);
  fail_if (err != NULL);

  kms_sdp_rtp_avp_media_handler_add_generic_video_payload
      (KMS_SDP_RTP_AVP_MEDIA_HANDLER (handler), "red/8000", &err);
  fail_if (err != NULL);

  id = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_if (id < 0);

  /* re-use handler for audio */
  g_object_ref (handler);
  id = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_if (id < 0);

  ctx = kms_sdp_agent_create_offer (offerer, &err);
  fail_if (err != NULL);

  offer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Offer:\n%s", (sdp_str = gst_sdp_message_as_text (offer)));
  g_free (sdp_str);

  ctx = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  answer = kms_sdp_message_context_pack (ctx, &err);
  fail_if (err != NULL);
  kms_sdp_message_context_destroy (ctx);

  GST_DEBUG ("Answer:\n%s", (sdp_str = gst_sdp_message_as_text (answer)));
  g_free (sdp_str);

  check_all_is_negotiated (offer, answer);
  check_payloads (answer, fec_payload, red_payload);

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);

  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST static Suite *
sdp_agent_suite (void)
{
  Suite *s = suite_create ("kmssdpagent");
  TCase *tc_chain = tcase_create ("SdpAgent");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, sdp_agent_test_create_offer);
  tcase_add_test (tc_chain, sdp_agent_test_add_proto_handler);
  tcase_add_test (tc_chain, sdp_agent_test_rejected_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rejected_unsupported_media);
  tcase_add_test (tc_chain, sdp_agent_test_sctp_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_avp_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_avpf_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_savpf_negotiation);
  tcase_add_test (tc_chain, sdp_agent_avp_generic_payload_negotiation);
  tcase_add_test (tc_chain, sdp_agent_avp_avpf_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_bundle_group);
  tcase_add_test (tc_chain, sdp_agent_test_fb_messages);
  tcase_add_test (tc_chain, sdp_agent_test_rtcp_mux);
  tcase_add_test (tc_chain, sdp_agent_test_multi_m_lines);
  tcase_add_test (tc_chain, sdp_agent_test_filter_unknown_attr);
  tcase_add_test (tc_chain, sdp_agent_test_supported_attrs);
  tcase_add_test (tc_chain, sdp_agent_test_bandwidtth_attrs);
  tcase_add_test (tc_chain, sdp_agent_test_extmap_attrs);
  tcase_add_test (tc_chain, sdp_agent_test_dynamic_pts);
  tcase_add_test (tc_chain, sdp_agent_test_optional_enc_parameters);
  tcase_add_test (tc_chain, sdp_agent_regression_tests);

  return s;
}

GST_CHECK_MAIN (sdp_agent)
