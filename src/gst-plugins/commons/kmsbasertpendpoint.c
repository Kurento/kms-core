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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "kmsbasertpendpoint.h"

#include <stdlib.h>

#include "kms-core-enumtypes.h"
#include "kms-core-marshal.h"
#include "sdp_utils.h"

#define PLUGIN_NAME "base_rtp_endpoint"

GST_DEBUG_CATEGORY_STATIC (kms_base_rtp_endpoint_debug);
#define GST_CAT_DEFAULT kms_base_rtp_endpoint_debug

#define kms_base_rtp_endpoint_parent_class parent_class
G_DEFINE_TYPE (KmsBaseRtpEndpoint, kms_base_rtp_endpoint,
    KMS_TYPE_BASE_SDP_ENDPOINT);

#define KMS_BASE_RTP_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                   \
    (obj),                                        \
    KMS_TYPE_BASE_RTP_ENDPOINT,                   \
    KmsBaseRtpEndpointPrivate                     \
  )                                               \
)

struct _KmsBaseRtpEndpointPrivate
{
  GstElement *rtpbin;

  gchar *proto;
  gboolean bundle;              /* Implies rtcp-mux */
  gboolean rtcp_mux;

  GstElement *audio_payloader;
  GstElement *video_payloader;

  guint local_audio_ssrc;
  guint audio_ssrc;

  guint local_video_ssrc;
  guint video_ssrc;

  gboolean negotiated;

  gint32 target_bitrate;
};

/* Signals and args */
enum
{
  MEDIA_START,
  MEDIA_STOP,
  LAST_SIGNAL
};

static guint obj_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_PROTO    NULL
#define DEFAULT_BUNDLE    FALSE
#define DEFAULT_TARGET_BITRATE    0

enum
{
  PROP_0,
  PROP_PROTO,
  PROP_BUNDLE,
  PROP_TARGET_BITRATE,
  PROP_LAST
};

/* Set Transport begin */
static gboolean
sdp_message_is_bundle (GstSDPMessage * msg)
{
  gboolean is_bundle = FALSE;
  guint i;

  for (i = 0;; i++) {
    const gchar *val;
    GRegex *regex;
    GMatchInfo *match_info = NULL;

    val = gst_sdp_message_get_attribute_val_n (msg, "group", i);
    if (val == NULL)
      break;

    regex = g_regex_new ("BUNDLE(?<mids>.*)?", 0, 0, NULL);
    g_regex_match (regex, val, 0, &match_info);
    g_regex_unref (regex);

    if (g_match_info_matches (match_info)) {
      gchar *mids_str = g_match_info_fetch_named (match_info, "mids");
      gchar **mids;

      mids = g_strsplit (mids_str, " ", 0);
      g_free (mids_str);
      is_bundle = g_strv_length (mids) > 0;
      g_strfreev (mids);
      g_match_info_free (match_info);

      break;
    }

    g_match_info_free (match_info);
  }

  return is_bundle;
}

static gboolean
sdp_message_is_rtcp_mux (GstSDPMessage * msg)
{
  guint len, i;

  len = gst_sdp_message_medias_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (msg, i);
    const gchar *val;

    val = gst_sdp_media_get_attribute_val (media, RTCP_MUX);
    if (val == NULL) {
      return FALSE;
    }
  }

  return TRUE;
}

static void
sdp_media_set_rtcp_fb_attrs (GstSDPMedia * media)
{
  guint i, f_len;

  if (g_strcmp0 (VIDEO_STREAM_NAME, gst_sdp_media_get_media (media)) != 0) {
    return;
  }

  f_len = gst_sdp_media_formats_len (media);

  for (i = 0; i < f_len; i++) {
    const gchar *pt = gst_sdp_media_get_format (media, i);
    gchar *enconding_name = gst_sdp_media_format_get_encoding_name (media, pt);

    if (g_ascii_strcasecmp (VP8_ENCONDING_NAME, enconding_name) == 0) {
      gchar *aux;

      aux = g_strconcat (pt, " ccm fir", NULL);
      gst_sdp_media_add_attribute (media, RTCP_FB, aux);
      g_free (aux);

      aux = g_strconcat (pt, " nack", NULL);
      gst_sdp_media_add_attribute (media, RTCP_FB, aux);
      g_free (aux);

      aux = g_strconcat (pt, " nack pli", NULL);
      gst_sdp_media_add_attribute (media, RTCP_FB, aux);
      g_free (aux);

      aux = g_strconcat (pt, " goog-remb", NULL);
      gst_sdp_media_add_attribute (media, RTCP_FB, aux);
      g_free (aux);
    }

    g_free (enconding_name);
  }
}

static gboolean
kms_base_rtp_endpoint_update_sdp_media (KmsBaseRtpEndpoint * self,
    GstSDPMedia * media, gboolean use_ipv6, const gchar ** media_str)
{
  const gchar *rtpbin_pad_name = NULL;
  guint session_id;
  gchar *rtp_addr, *rtcp_addr;
  const gchar *rtp_addr_type, *rtcp_addr_type;
  guint rtp_port, rtcp_port;
  guint conn_len, c;
  gchar *str;

  *media_str = gst_sdp_media_get_media (media);
  if (self->priv->bundle) {
    if (g_strcmp0 (AUDIO_STREAM_NAME, *media_str) == 0) {
      rtpbin_pad_name = AUDIO_RTPBIN_SEND_RTP_SINK;
      session_id = AUDIO_RTP_SESSION;
    } else if (g_strcmp0 (VIDEO_STREAM_NAME, *media_str) == 0) {
      rtpbin_pad_name = VIDEO_RTPBIN_SEND_RTP_SINK;
      session_id = VIDEO_RTP_SESSION;
    } else {
      GST_WARNING_OBJECT (self, "Media \"%s\" not supported", *media_str);
      *media_str = NULL;
      return TRUE;
    }
  }

  gst_sdp_media_set_proto (media, self->priv->proto);

  rtp_addr_type = use_ipv6 ? "IP6" : "IP4";
  rtcp_addr_type = use_ipv6 ? "IP6" : "IP4";

  rtp_port = rtcp_port = 1;
  rtp_addr = rtcp_addr = "0.0.0.0";

  ((GstSDPMedia *) media)->port = rtp_port;
  conn_len = gst_sdp_media_connections_len (media);
  for (c = 0; c < conn_len; c++) {
    gst_sdp_media_remove_connection ((GstSDPMedia *) media, c);
  }
  gst_sdp_media_add_connection ((GstSDPMedia *) media, "IN", rtp_addr_type,
      rtp_addr, 0, 0);

  str = g_strdup_printf ("%d IN %s %s", rtcp_port, rtcp_addr_type, rtcp_addr);
  gst_sdp_media_add_attribute ((GstSDPMedia *) media, "rtcp", str);
  g_free (str);

  if (self->priv->bundle || self->priv->rtcp_mux) {
    gst_sdp_media_add_attribute ((GstSDPMedia *) media, RTCP_MUX, "");
  }

  if (rtpbin_pad_name != NULL) {
    GstElement *rtpbin = self->priv->rtpbin;
    GObject *rtpsession;
    GstPad *pad;

    /* Create RtpSession requesting the pad */
    pad = gst_element_get_request_pad (rtpbin, rtpbin_pad_name);
    g_object_unref (pad);

    g_signal_emit_by_name (rtpbin, "get-internal-session", session_id,
        &rtpsession);
    if (rtpsession != NULL) {
      guint ssrc;
      gchar *value;
      GstStructure *sdes;

      g_object_get (rtpsession, "internal-ssrc", &ssrc, NULL);
      g_object_unref (rtpsession);

      g_object_get (rtpbin, "sdes", &sdes, NULL);
      value =
          g_strdup_printf ("%" G_GUINT32_FORMAT " cname:%s", ssrc,
          gst_structure_get_string (sdes, "cname"));
      gst_structure_free (sdes);
      gst_sdp_media_add_attribute (media, "ssrc", value);
      g_free (value);

      if (session_id == AUDIO_RTP_SESSION) {
        self->priv->local_audio_ssrc = ssrc;
      } else if (session_id == VIDEO_RTP_SESSION) {
        self->priv->local_video_ssrc = ssrc;
      }
    }
  }

  sdp_media_set_rtcp_fb_attrs (media);

  return TRUE;
}

static gboolean
kms_base_rtp_endpoint_set_transport_to_sdp (KmsBaseSdpEndpoint *
    base_sdp_endpoint, GstSDPMessage * msg)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  GstSDPMessage *remote_offer_sdp;
  guint len, i;
  gchar *bundle_mids = NULL;
  gboolean ret = TRUE;

  KMS_ELEMENT_LOCK (self);
  g_object_get (base_sdp_endpoint, "remote-offer-sdp", &remote_offer_sdp, NULL);
  if (remote_offer_sdp != NULL) {
    self->priv->bundle = sdp_message_is_bundle (remote_offer_sdp);
    self->priv->rtcp_mux = sdp_message_is_rtcp_mux (remote_offer_sdp);
    gst_sdp_message_free (remote_offer_sdp);
  }

  GST_INFO ("BUNDLE: %" G_GUINT32_FORMAT, self->priv->bundle);

  if (self->priv->bundle) {
    bundle_mids = g_strdup ("BUNDLE");
  }

  len = gst_sdp_message_medias_len (msg);
  for (i = 0; i < len; i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (msg, i);
    const gchar *media_str = NULL;
    gboolean use_ipv6;

    g_object_get (base_sdp_endpoint, "use-ipv6", &use_ipv6, NULL);
    if (!kms_base_rtp_endpoint_update_sdp_media (self, (GstSDPMedia *) media,
            use_ipv6, &media_str)) {
      ret = FALSE;
      goto end;
    }

    if (media_str == NULL) {
      continue;
    }

    if (self->priv->bundle) {
      gchar *tmp;

      tmp = g_strconcat (bundle_mids, " ", media_str, NULL);
      g_free (bundle_mids);
      bundle_mids = tmp;
    }
  }

  if (self->priv->bundle) {
    gst_sdp_message_add_attribute (msg, "group", bundle_mids);
    g_free (bundle_mids);
  }

end:
  KMS_ELEMENT_UNLOCK (self);

  return ret;
}

/* Set Transport end */

static const gchar *
get_caps_codec_name (const gchar * codec_name)
{
  // TODO: Add more special cases here

  if (g_ascii_strcasecmp (OPUS_ENCONDING_NAME, codec_name) == 0) {
    return "X-GST-OPUS-DRAFT-SPITTKA-00";
  } else if (g_ascii_strcasecmp (VP8_ENCONDING_NAME, codec_name) == 0) {
    return "VP8-DRAFT-IETF-01";
  } else {
    return codec_name;
  }
}

static GstCaps *
kms_base_rtp_endpoint_get_caps_from_rtpmap (const gchar * media,
    const gchar * pt, const gchar * rtpmap)
{
  GstCaps *caps = NULL;
  gchar **tokens;

  if (rtpmap == NULL) {
    GST_WARNING ("rtpmap is NULL");
    return NULL;
  }

  tokens = g_strsplit (rtpmap, "/", 3);

  if (tokens[0] == NULL || tokens[1] == NULL) {
    goto end;
  }

  caps = gst_caps_new_simple ("application/x-rtp",
      "media", G_TYPE_STRING, media,
      "payload", G_TYPE_INT, atoi (pt),
      "clock-rate", G_TYPE_INT, atoi (tokens[1]),
      "encoding-name", G_TYPE_STRING, get_caps_codec_name (tokens[0]), NULL);

end:
  g_strfreev (tokens);

  return caps;
}

static GstElement *
gst_base_rtp_get_payloader_for_caps (GstCaps * caps)
{
  GstElementFactory *factory;
  GstElement *payloader = NULL;
  GList *payloader_list, *filtered_list;
  GParamSpec *pspec;

  payloader_list =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_PAYLOADER,
      GST_RANK_NONE);
  filtered_list =
      gst_element_factory_list_filter (payloader_list, caps, GST_PAD_SRC,
      FALSE);

  if (filtered_list == NULL) {
    goto end;
  }

  factory = GST_ELEMENT_FACTORY (filtered_list->data);
  if (factory == NULL) {
    goto end;
  }

  payloader = gst_element_factory_create (factory, NULL);

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (payloader), "pt");
  if (pspec != NULL && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_UINT) {
    GstStructure *st = gst_caps_get_structure (caps, 0);
    gint payload;

    if (gst_structure_get_int (st, "payload", &payload)) {
      g_object_set (payloader, "pt", payload, NULL);
    }
  }

  pspec =
      g_object_class_find_property (G_OBJECT_GET_CLASS (payloader),
      "config-interval");
  if (pspec != NULL && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_UINT) {
    g_object_set (payloader, "config-interval", 1, NULL);
  }

end:
  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (payloader_list);

  return payloader;
}

static GstElement *
gst_base_rtp_get_depayloader_for_caps (GstCaps * caps)
{
  GstElementFactory *factory;
  GstElement *depayloader = NULL;
  GList *payloader_list, *filtered_list, *l;

  payloader_list =
      gst_element_factory_list_get_elements
      (GST_ELEMENT_FACTORY_TYPE_DEPAYLOADER, GST_RANK_NONE);
  filtered_list =
      gst_element_factory_list_filter (payloader_list, caps, GST_PAD_SINK,
      FALSE);

  if (filtered_list == NULL) {
    goto end;
  }

  for (l = filtered_list; l != NULL; l = l->next) {
    factory = GST_ELEMENT_FACTORY (l->data);

    if (factory == NULL) {
      continue;
    }

    if (g_strcmp0 (gst_plugin_feature_get_name (factory), "asteriskh263") == 0) {
      /* Do not use asteriskh263 for H263 */
      continue;
    }

    depayloader = gst_element_factory_create (factory, NULL);

    if (depayloader != NULL) {
      break;
    }
  }

end:
  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (payloader_list);

  return depayloader;
}

static void
kms_base_rtp_endpoint_connect_payloader (KmsBaseRtpEndpoint * ep,
    KmsElementPadType type, GstElement * payloader,
    const gchar * rtpbin_pad_name)
{
  GstElement *rtprtxqueue = gst_element_factory_make ("rtprtxqueue", NULL);
  GstPad *target;

  g_object_set (rtprtxqueue, "max-size-packets", 128, NULL);

  g_object_ref (payloader);
  gst_bin_add_many (GST_BIN (ep), payloader, rtprtxqueue, NULL);
  gst_element_sync_state_with_parent (payloader);
  gst_element_sync_state_with_parent (rtprtxqueue);

  gst_element_link (payloader, rtprtxqueue);
  gst_element_link_pads (rtprtxqueue, "src", ep->priv->rtpbin, rtpbin_pad_name);

  target = gst_element_get_static_pad (payloader, "sink");
  kms_element_connect_sink_target (KMS_ELEMENT (ep), target, type);
  g_object_unref (target);
}

static void
kms_base_rtp_endpoint_connect_input_elements (KmsBaseSdpEndpoint *
    base_endpoint, const GstSDPMessage * answer)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_endpoint);
  guint i, len;

  KMS_BASE_SDP_ENDPOINT_CLASS
      (kms_base_rtp_endpoint_parent_class)->connect_input_elements
      (base_endpoint, answer);
  GST_DEBUG ("connect_input_elements");

  if (answer == NULL) {
    GST_ERROR ("Asnwer is NULL");
    return;
  }

  len = gst_sdp_message_medias_len (answer);

  KMS_ELEMENT_LOCK (base_endpoint);
  self->priv->negotiated = TRUE;

  for (i = 0; i < len; i++) {
    const gchar *proto_str;
    GstElement *payloader;
    GstCaps *caps = NULL;
    const gchar *rtpmap;
    const GstSDPMedia *media = gst_sdp_message_get_media (answer, i);
    guint j, f_len;

    proto_str = gst_sdp_media_get_proto (media);
    if (g_strcmp0 (proto_str, self->priv->proto)) {
      GST_WARNING_OBJECT (self, "Proto \"%s\" not supported", proto_str);
      continue;
    }

    f_len = gst_sdp_media_formats_len (media);

    for (j = 0; j < f_len && caps == NULL; j++) {
      const gchar *pt = gst_sdp_media_get_format (media, j);

      rtpmap = sdp_utils_sdp_media_get_rtpmap (media, pt);
      caps =
          kms_base_rtp_endpoint_get_caps_from_rtpmap (gst_sdp_media_get_media
          (media), pt, rtpmap);
    }

    if (caps == NULL) {
      continue;
    }

    GST_DEBUG ("Found caps: %" GST_PTR_FORMAT, caps);

    payloader = gst_base_rtp_get_payloader_for_caps (caps);
    if (payloader != NULL) {
      KmsBaseRtpEndpoint *rtp_endpoint = KMS_BASE_RTP_ENDPOINT (base_endpoint);
      const gchar *rtpbin_pad_name;
      KmsElementPadType type;

      GST_DEBUG ("Found payloader %" GST_PTR_FORMAT, payloader);
      if (g_strcmp0 ("audio", gst_sdp_media_get_media (media)) == 0) {
        rtp_endpoint->priv->audio_payloader = payloader;
        type = KMS_ELEMENT_PAD_TYPE_AUDIO;
        rtpbin_pad_name = AUDIO_RTPBIN_SEND_RTP_SINK;
      } else if (g_strcmp0 ("video", gst_sdp_media_get_media (media)) == 0) {
        rtp_endpoint->priv->video_payloader = payloader;
        type = KMS_ELEMENT_PAD_TYPE_VIDEO;
        rtpbin_pad_name = VIDEO_RTPBIN_SEND_RTP_SINK;
      } else {
        rtpbin_pad_name = NULL;
        g_object_unref (payloader);
      }

      if (rtpbin_pad_name != NULL) {
        kms_base_rtp_endpoint_connect_payloader (rtp_endpoint, type, payloader,
            rtpbin_pad_name);
      }
    }

    gst_caps_unref (caps);
  }

  KMS_ELEMENT_UNLOCK (base_endpoint);
}

static GstCaps *
kms_base_rtp_endpoint_get_caps_for_pt (KmsBaseRtpEndpoint * self, guint pt)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  GstSDPMessage *answer;
  guint i, len;
  GstCaps *ret = NULL;

  g_object_get (base_endpoint, "local-answer-sdp", &answer, NULL);
  if (answer == NULL)
    g_object_get (base_endpoint, "remote-answer-sdp", &answer, NULL);

  if (answer == NULL) {
    return NULL;
  }

  len = gst_sdp_message_medias_len (answer);

  for (i = 0; i < len; i++) {
    const gchar *rtpmap;
    const GstSDPMedia *media = gst_sdp_message_get_media (answer, i);
    guint j, f_len;

    f_len = gst_sdp_media_formats_len (media);

    for (j = 0; j < f_len; j++) {
      GstCaps *caps;
      const gchar *payload = gst_sdp_media_get_format (media, j);

      if (atoi (payload) != pt) {
        continue;
      }

      rtpmap = sdp_utils_sdp_media_get_rtpmap (media, payload);
      caps =
          kms_base_rtp_endpoint_get_caps_from_rtpmap (gst_sdp_media_get_media
          (media), payload, rtpmap);

      if (caps != NULL) {
        if (g_strcmp0 ("video", gst_sdp_media_get_media (media)) == 0) {
          gst_caps_set_simple (caps, "rtcp-fb-ccm-fir",
              G_TYPE_BOOLEAN, TRUE, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, TRUE,
              NULL);
        }

        ret = caps;
        goto end;
      }
    }
  }

end:
  gst_sdp_message_free (answer);

  return ret;
}

static GstCaps *
kms_base_rtp_endpoint_request_pt_map (GstElement * rtpbin, guint session,
    guint pt, KmsBaseRtpEndpoint * base_rtp_endpoint)
{
  GstCaps *caps;

  GST_DEBUG ("Caps request for pt: %d", pt);

  caps = kms_base_rtp_endpoint_get_caps_for_pt (base_rtp_endpoint, pt);

  if (caps != NULL) {
    return caps;
  }

  return gst_caps_new_simple ("application/x-rtp", "payload", G_TYPE_INT, pt,
      "rtcp-fb-ccm-fir",
      G_TYPE_BOOLEAN, TRUE, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, TRUE, NULL);
}

static void
kms_base_rtp_endpoint_rtpbin_pad_added (GstElement * rtpbin, GstPad * pad,
    KmsBaseRtpEndpoint * rtp_endpoint)
{
  GstElement *agnostic, *depayloader;
  gboolean added = TRUE;
  KmsMediaType media;
  GstCaps *caps;

  GST_PAD_STREAM_LOCK (pad);

  if (g_str_has_prefix (GST_OBJECT_NAME (pad), AUDIO_RTPBIN_RECV_RTP_SRC)) {
    agnostic = kms_element_get_audio_agnosticbin (KMS_ELEMENT (rtp_endpoint));
    media = KMS_MEDIA_TYPE_AUDIO;
  } else if (g_str_has_prefix (GST_OBJECT_NAME (pad),
          VIDEO_RTPBIN_RECV_RTP_SRC)) {
    agnostic = kms_element_get_video_agnosticbin (KMS_ELEMENT (rtp_endpoint));
    media = KMS_MEDIA_TYPE_VIDEO;
  } else {
    added = FALSE;
    goto end;
  }

  caps = gst_pad_query_caps (pad, NULL);
  GST_DEBUG ("New pad: %" GST_PTR_FORMAT " for linking to %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, pad, agnostic, caps);

  depayloader = gst_base_rtp_get_depayloader_for_caps (caps);

  if (caps != NULL) {
    gst_caps_unref (caps);
  }

  if (depayloader != NULL) {
    GST_DEBUG ("Found depayloader %" GST_PTR_FORMAT, depayloader);

    gst_bin_add (GST_BIN (rtp_endpoint), depayloader);
    gst_element_sync_state_with_parent (depayloader);

    gst_element_link_pads (depayloader, "src", agnostic, "sink");
    gst_element_link_pads (rtpbin, GST_OBJECT_NAME (pad), depayloader, "sink");
  } else {
    GstElement *fake = gst_element_factory_make ("fakesink", NULL);

    gst_bin_add (GST_BIN (rtp_endpoint), fake);
    gst_element_sync_state_with_parent (fake);
    gst_element_link_pads (rtpbin, GST_OBJECT_NAME (pad), fake, "sink");
  }

end:
  GST_PAD_STREAM_UNLOCK (pad);

  if (added) {
    g_signal_emit (G_OBJECT (rtp_endpoint), obj_signals[MEDIA_START], 0, media,
        TRUE);
  }
}

static void
kms_base_rtp_endpoint_rtpbin_new_jitterbuffer (GstElement * rtpbin,
    GstElement * jitterbuffer,
    guint session, guint ssrc, KmsBaseRtpEndpoint * rtp_endpoint)
{
  g_object_set (jitterbuffer, "mode", 4 /* synced */ , "latency", 1500, NULL);

  if (ssrc == rtp_endpoint->priv->video_ssrc) {
    g_object_set (jitterbuffer, "do-lost", TRUE,
        "do-retransmission", TRUE,
        "rtx-next-seqnum", FALSE,
        "rtx-max-retries", 0, "rtp-max-dropout", -1, NULL);
  }
}

static void
kms_base_rtp_endpoint_stop_signal (KmsBaseRtpEndpoint * self, guint session,
    guint ssrc)
{
  gboolean local = TRUE;
  KmsMediaType media;

  KMS_ELEMENT_LOCK (self);

  if (ssrc == self->priv->audio_ssrc || ssrc == self->priv->video_ssrc) {
    local = FALSE;

    if (self->priv->audio_ssrc == ssrc)
      self->priv->audio_ssrc = 0;
    else if (self->priv->video_ssrc == ssrc)
      self->priv->video_ssrc = 0;
  }

  KMS_ELEMENT_UNLOCK (self);

  switch (session) {
    case AUDIO_RTP_SESSION:
      media = KMS_MEDIA_TYPE_AUDIO;
      break;
    case VIDEO_RTP_SESSION:
      media = KMS_MEDIA_TYPE_VIDEO;
      break;
    default:
      GST_WARNING_OBJECT (self, "No media supported for session %u", session);
      return;
  }

  g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_STOP], 0, media, local);
}

static void
kms_base_rtp_endpoint_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (property_id) {
    case PROP_PROTO:{
      const gchar *str = g_value_get_string (value);

      g_free (self->priv->proto);
      self->priv->proto = g_strdup (str);
      break;
    }
    case PROP_BUNDLE:
      self->priv->bundle = g_value_get_boolean (value);
      break;
    case PROP_TARGET_BITRATE:
      self->priv->target_bitrate = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_bse_rtp_endpoint_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (property_id) {
    case PROP_PROTO:
      g_value_set_string (value, self->priv->proto);
      break;
    case PROP_BUNDLE:
      g_value_set_boolean (value, self->priv->bundle);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_int (value, self->priv->target_bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_rtp_endpoint_dispose (GObject * gobject)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (gobject);

  if (self->priv->audio_payloader != NULL) {
    g_object_unref (self->priv->audio_payloader);
    self->priv->audio_payloader = NULL;
  }

  if (self->priv->video_payloader != NULL) {
    g_object_unref (self->priv->video_payloader);
    self->priv->video_payloader = NULL;
  }

  if (self->priv->audio_ssrc != 0) {
    kms_base_rtp_endpoint_stop_signal (self, AUDIO_RTP_SESSION,
        self->priv->audio_ssrc);
    g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_STOP], 0,
        KMS_MEDIA_TYPE_AUDIO, TRUE);
  }

  if (self->priv->video_ssrc != 0) {
    kms_base_rtp_endpoint_stop_signal (self, VIDEO_RTP_SESSION,
        self->priv->video_ssrc);
    g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_STOP], 0,
        KMS_MEDIA_TYPE_VIDEO, TRUE);
  }

  G_OBJECT_CLASS (kms_base_rtp_endpoint_parent_class)->dispose (gobject);
}

static void
kms_base_rtp_endpoint_finalize (GObject * gobject)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (gobject);

  GST_DEBUG_OBJECT (self, "finalize");

  g_free (self->priv->proto);

  G_OBJECT_CLASS (kms_base_rtp_endpoint_parent_class)->finalize (gobject);
}

static void
kms_base_rtp_endpoint_class_init (KmsBaseRtpEndpointClass * klass)
{
  KmsBaseSdpEndpointClass *base_endpoint_class;
  GstElementClass *gstelement_class;
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = kms_base_rtp_endpoint_dispose;
  object_class->finalize = kms_base_rtp_endpoint_finalize;
  object_class->set_property = kms_base_rtp_endpoint_set_property;
  object_class->get_property = kms_bse_rtp_endpoint_get_property;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "BaseRtpEndpoint",
      "Base/Bin/BaseRtpEndpoints",
      "Base class for RtpEndpoints",
      "José Antonio Santos Cadenas <santoscadenas@kurento.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  base_endpoint_class = KMS_BASE_SDP_ENDPOINT_CLASS (klass);
  base_endpoint_class->set_transport_to_sdp =
      kms_base_rtp_endpoint_set_transport_to_sdp;
  base_endpoint_class->connect_input_elements =
      kms_base_rtp_endpoint_connect_input_elements;

  g_object_class_install_property (object_class, PROP_PROTO,
      g_param_spec_string ("proto", "RTP/RTCP protocol",
          "RTP/RTCP protocol", DEFAULT_PROTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_BUNDLE,
      g_param_spec_boolean ("bundle", "Bundle media",
          "Bundle media", DEFAULT_BUNDLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_TARGET_BITRATE,
      g_param_spec_int ("target-bitrate", "Target bitrate",
          "Target bitrate (bps)", 0, G_MAXINT,
          DEFAULT_TARGET_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* set signals */
  obj_signals[MEDIA_START] =
      g_signal_new ("media-start",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, media_start), NULL, NULL,
      __kms_core_marshal_VOID__ENUM_BOOLEAN, G_TYPE_NONE, 2,
      KMS_TYPE_MEDIA_TYPE, G_TYPE_BOOLEAN);

  obj_signals[MEDIA_STOP] =
      g_signal_new ("media-stop",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, media_stop), NULL, NULL,
      __kms_core_marshal_VOID__ENUM_BOOLEAN, G_TYPE_NONE, 2,
      KMS_TYPE_MEDIA_TYPE, G_TYPE_BOOLEAN);

  g_type_class_add_private (klass, sizeof (KmsBaseRtpEndpointPrivate));
}

static void
kms_base_rtp_endpoint_rtpbin_on_new_ssrc (GstElement * rtpbin, guint session,
    guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  KMS_ELEMENT_LOCK (self);

  switch (session) {
    case AUDIO_RTP_SESSION:
      if (self->priv->audio_ssrc != 0)
        break;

      self->priv->audio_ssrc = ssrc;
      break;
    case VIDEO_RTP_SESSION:
      if (self->priv->video_ssrc != 0)
        break;

      self->priv->video_ssrc = ssrc;
      break;
    default:
      GST_WARNING_OBJECT (self, "No media supported for session %u", session);
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_rtp_endpoint_rtpbin_on_bye_ssrc (GstElement * rtpbin, guint session,
    guint ssrc, gpointer user_data)
{
  kms_base_rtp_endpoint_stop_signal (KMS_BASE_RTP_ENDPOINT (user_data),
      session, ssrc);
}

static void
kms_base_rtp_endpoint_rtpbin_on_bye_timeout (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  kms_base_rtp_endpoint_stop_signal (KMS_BASE_RTP_ENDPOINT (user_data),
      session, ssrc);
}

static void
kms_base_rtp_endpoint_rtpbin_on_ssrc_sdes (GstElement * rtpbin, guint session,
    guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);
  KmsMediaType media;
  GObject *rtpsession;

  KMS_ELEMENT_LOCK (self);

  if (ssrc != self->priv->audio_ssrc && ssrc != self->priv->video_ssrc) {
    GST_WARNING_OBJECT (self, "SSRC %u not valid", ssrc);
    KMS_ELEMENT_UNLOCK (self);
    return;
  }

  KMS_ELEMENT_UNLOCK (self);

  switch (session) {
    case AUDIO_RTP_SESSION:
      media = KMS_MEDIA_TYPE_AUDIO;
      break;
    case VIDEO_RTP_SESSION:
      media = KMS_MEDIA_TYPE_VIDEO;
      break;
    default:
      GST_WARNING_OBJECT (self, "No media supported for session %u", session);
      return;
  }

  g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_START], 0, media, FALSE);

  g_signal_emit_by_name (rtpbin, "get-internal-session", session, &rtpsession);
  if (rtpsession != NULL) {
    g_object_set (rtpsession, "rtcp-min-interval",
        RTCP_MIN_INTERVAL * GST_MSECOND, NULL);
    g_object_unref (rtpsession);
  }
}

static void
kms_base_rtp_endpoint_rtpbin_on_sender_timeout (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  kms_base_rtp_endpoint_stop_signal (KMS_BASE_RTP_ENDPOINT (user_data),
      session, ssrc);
}

static void
kms_base_rtp_endpoint_init (KmsBaseRtpEndpoint * self)
{
  self->priv = KMS_BASE_RTP_ENDPOINT_GET_PRIVATE (self);
  self->priv->proto = DEFAULT_PROTO;
  self->priv->bundle = DEFAULT_BUNDLE;

  self->priv->rtpbin = gst_element_factory_make ("rtpbin", NULL);

  g_signal_connect (self->priv->rtpbin, "request-pt-map",
      G_CALLBACK (kms_base_rtp_endpoint_request_pt_map), self);

  g_signal_connect (self->priv->rtpbin, "pad-added",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_pad_added), self);

  g_signal_connect (self->priv->rtpbin, "on-new-ssrc",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_new_ssrc), self);
  g_signal_connect (self->priv->rtpbin, "on-ssrc-sdes",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_ssrc_sdes), self);
  g_signal_connect (self->priv->rtpbin, "on-bye-ssrc",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_bye_ssrc), self);
  g_signal_connect (self->priv->rtpbin, "on-bye-timeout",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_bye_timeout), self);
  g_signal_connect (self->priv->rtpbin, "on-sender-timeout",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_sender_timeout), self);

  g_signal_connect (self->priv->rtpbin, "new-jitterbuffer",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_new_jitterbuffer), self);

  g_object_set (self, "accept-eos", FALSE, "do-synchronization", TRUE, NULL);

  gst_bin_add (GST_BIN (self), self->priv->rtpbin);

  self->priv->audio_payloader = NULL;
  self->priv->video_payloader = NULL;
  self->priv->negotiated = FALSE;
}

GstElement *
kms_base_rtp_endpoint_get_rtpbin (KmsBaseRtpEndpoint * self)
{
  g_return_val_if_fail (KMS_IS_BASE_RTP_ENDPOINT (self), NULL);

  return self->priv->rtpbin;
}
