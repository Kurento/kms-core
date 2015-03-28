/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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

#include <stdlib.h>

#include "kmssdpagent.h"
#include "sdp_utils.h"
#include "kmssdprtpavpmediahandler.h"

#define OBJECT_NAME "rtpavpmediahandler"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_rtp_avp_media_handler_debug_category);
#define GST_CAT_DEFAULT kms_sdp_rtp_avp_media_handler_debug_category

#define parent_class kms_sdp_rtp_avp_media_handler_parent_class

G_DEFINE_TYPE_WITH_CODE (KmsSdpRtpAvpMediaHandler,
    kms_sdp_rtp_avp_media_handler, KMS_TYPE_SDP_MEDIA_HANDLER,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_rtp_avp_media_handler_debug_category,
        OBJECT_NAME, 0, "debug category for sdp rtp avp media_handler"));

#define SDP_MEDIA_RTP_AVP_PROTO "RTP/AVP"
#define SDP_AUDIO_MEDIA "audio"
#define SDP_VIDEO_MEDIA "video"

#define DEFAULT_RTP_AUDIO_BASE_PAYLOAD 0
#define DEFAULT_RTP_VIDEO_BASE_PAYLOAD 24

/* Table extracted from rfc3551 [6] */
static gchar *rtpmaps[] = {
  /* Payload types (PT) for audio encodings */
  "PCMU/8000/1",
  NULL,                         /* reserved */
  NULL,                         /* reserved */
  "GSM/8000/1",
  "G723/8000/1",
  "DVI4/8000/1",
  "DVI4/16000/1",
  "LPC/8000/1",
  "PCMA/8000/1",
  "G722/8000/1",
  "L16/44100/2",
  "L16/44100/1",
  "QCELP/8000/1",
  "CN/8000/1",
  "MPA/90000",
  "G728/8000/1",
  "DVI4/11025/1",
  "DVI4/22050/1",
  "G729/8000/1",
  NULL,                         /* reserved */
  NULL,                         /* unasigned */
  NULL,                         /* unasigned */
  NULL,                         /* unasigned */
  NULL,                         /* unasigned */

  /* Payload types (PT) for video encodings */
  NULL,                         /* unasigned */
  "CelB/90000",
  "JPEG/90000",
  NULL,                         /* unasigned */
  "nv/90000",
  NULL,                         /* unasigned */
  NULL,                         /* unasigned */
  "H261/90000",
  "MPV/90000",
  "MP2T/90000",
  "H263/90000",
};

#define RESERVED_PAYLOAD_TYPE()
typedef struct _KmsSdpRtpMap KmsSdpRtpMap;
struct _KmsSdpRtpMap
{
  guint payload;
  gchar *name;
};

/* TODO: Make these lists configurable */
static KmsSdpRtpMap audio_fmts[] = {
  {98, "OPUS/48000/2"},
  {99, "AMR/8000/1"},
  {0, "PCMU/8000"}
};

static KmsSdpRtpMap video_fmts[] = {
  {96, "H263-1998/90000"},
  {97, "VP8/90000"},
  {100, "MP4V-ES/90000"},
  {101, "H264/90000"}
};

static GObject *
kms_sdp_rtp_avp_media_handler_constructor (GType gtype, guint n_properties,
    GObjectConstructParam * properties)
{
  GObjectConstructParam *property;
  gchar const *name;
  GObject *object;
  guint i;

  for (i = 0, property = properties; i < n_properties; ++i, ++property) {
    name = g_param_spec_get_name (property->pspec);
    if (g_strcmp0 (name, "proto") == 0) {
      if (g_value_get_string (property->value) == NULL) {
        /* change G_PARAM_CONSTRUCT_ONLY value */
        g_value_set_string (property->value, SDP_MEDIA_RTP_AVP_PROTO);
      }
    }
  }

  object =
      G_OBJECT_CLASS (parent_class)->constructor (gtype, n_properties,
      properties);

  return object;
}

static gboolean
kms_sdp_rtp_avp_media_handler_add_supported_fmts (GstSDPMedia * media,
    GError ** error)
{
  KmsSdpRtpMap *maps;
  gboolean is_audio;
  guint i, len;

  if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_AUDIO_MEDIA) == 0) {
    len = G_N_ELEMENTS (audio_fmts);
    maps = audio_fmts;
    is_audio = TRUE;
  } else if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_VIDEO_MEDIA) == 0) {
    len = G_N_ELEMENTS (video_fmts);
    maps = video_fmts;
    is_audio = FALSE;
  } else {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unsuported media %s", gst_sdp_media_get_media (media));
    return FALSE;
  }

  for (i = 0; i < len; i++) {
    KmsSdpRtpMap rtpmap;
    gchar *fmt;

    rtpmap = maps[i];

    /* Make some checks for default PTs */
    if (rtpmap.payload >= DEFAULT_RTP_AUDIO_BASE_PAYLOAD &&
        rtpmap.payload <= G_N_ELEMENTS (rtpmaps)) {
      if (rtpmaps[rtpmap.payload] == NULL) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Trying to use an invalid PT (%d)", rtpmap.payload);
      } else if (is_audio && rtpmap.payload >= DEFAULT_RTP_VIDEO_BASE_PAYLOAD) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Trying to use a reserved video payload type for audio (%d)",
            rtpmap.payload);
        return FALSE;
      } else if (!is_audio && rtpmap.payload < DEFAULT_RTP_VIDEO_BASE_PAYLOAD) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Trying to use a reserved audio payload type for video (%d)",
            rtpmap.payload);
        return FALSE;
      } else {
        gchar **codec;
        gboolean ret;

        codec = g_strsplit (rtpmap.name, "/", 0);

        ret = g_str_has_prefix (rtpmaps[rtpmap.payload], codec[0]);
        g_strfreev (codec);

        if (!ret) {
          g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
              "Trying to use a reserved payload %d for %s", rtpmap.payload,
              rtpmap.name);
          return FALSE;
        }
      }
    }

    fmt = g_strdup_printf ("%u", rtpmap.payload);

    if (gst_sdp_media_add_format (media, fmt) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Unable to set format %u", rtpmap.payload);
      g_free (fmt);
      return FALSE;
    }

    g_free (fmt);
  }

  return TRUE;
}

static gboolean
kms_sdp_rtp_avp_media_handler_add_rtpmap_attrs (GstSDPMedia * media,
    GError ** error)
{
  KmsSdpRtpMap *maps;
  guint i, len;

  if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_AUDIO_MEDIA) == 0) {
    len = G_N_ELEMENTS (audio_fmts);
    maps = audio_fmts;
  } else if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_VIDEO_MEDIA) == 0) {
    len = G_N_ELEMENTS (video_fmts);
    maps = video_fmts;
  } else {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unsuported media %s", gst_sdp_media_get_media (media));
    return FALSE;
  }

  for (i = 0; i < media->fmts->len; i++) {
    gchar *payload, *attr;
    guint pt, j;

    payload = g_array_index (media->fmts, gchar *, i);
    pt = atoi (payload);

    if (pt >= DEFAULT_RTP_AUDIO_BASE_PAYLOAD && pt <= G_N_ELEMENTS (rtpmaps)) {
      /* [rfc4566] rtpmap attribute can be omitted for static payload type  */
      /* numbers so it is completely defined in the RTP Audio/Video profile */
      continue;
    }

    for (j = 0; j < len; j++) {
      if (pt != maps[j].payload) {
        continue;
      }

      attr = g_strdup_printf ("%u %s", maps[j].payload, maps[j].name);

      if (gst_sdp_media_add_attribute (media, "rtpmap", attr) != GST_SDP_OK) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Unable to set attribute rtpmap:%s", attr);
        g_free (attr);
        return FALSE;
      }

      g_free (attr);
    }
  }

  return TRUE;
}

static GstSDPMedia *
kms_sdp_rtp_avp_media_handler_create_offer (KmsSdpMediaHandler * handler,
    const gchar * media, GError ** error)
{
  GstSDPMedia *m = NULL;
  gchar *proto = NULL;

  if (g_strcmp0 (media, SDP_AUDIO_MEDIA) != 0
      && g_strcmp0 (media, SDP_VIDEO_MEDIA) != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported %s media", media);
    goto error;
  }

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to create %s media", media);
    goto error;
  }

  if (gst_sdp_media_set_media (m, media) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set %s media", media);
    goto error;
  }

  g_object_get (handler, "proto", &proto, NULL);

  if (gst_sdp_media_set_proto (m, proto) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set %s protocol", SDP_MEDIA_RTP_AVP_PROTO);
    goto error;
  }

  if (gst_sdp_media_set_port_info (m, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set port");
    goto error;
  }

  if (!kms_sdp_rtp_avp_media_handler_add_supported_fmts (m, error)) {
    goto error;
  }

  if (!kms_sdp_rtp_avp_media_handler_add_rtpmap_attrs (m, error)) {
    goto error;
  }

  g_free (proto);

  return m;

error:
  if (m != NULL) {
    gst_sdp_media_free (m);
  }

  g_free (proto);

  return NULL;
}

static gboolean
encoding_supported (const GstSDPMedia * media, const gchar * enc)
{
  KmsSdpRtpMap *maps;
  guint i, len;

  if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_AUDIO_MEDIA) == 0) {
    len = G_N_ELEMENTS (audio_fmts);
    maps = audio_fmts;
  } else if (g_strcmp0 (gst_sdp_media_get_media (media), SDP_VIDEO_MEDIA) == 0) {
    len = G_N_ELEMENTS (video_fmts);
    maps = video_fmts;
  } else {
    return FALSE;
  }

  for (i = 0; i < len; i++) {
    if (g_str_has_prefix (enc, maps[i].name)) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
format_supported (const GstSDPMedia * media, const gchar * fmt)
{
  const gchar *val;
  gchar **attrs;
  gboolean ret;

  val = sdp_utils_get_attr_map_value (media, "rtpmap", fmt);

  if (val == NULL) {
    gint pt;

    /* Check if this is a static payload type so they do not need to be */
    /* set in an rtpmap attribute */

    pt = atoi (fmt);
    if (pt >= 0 && pt <= G_N_ELEMENTS (rtpmaps) && rtpmaps[pt] != NULL) {
      return encoding_supported (media, rtpmaps[pt]);
    } else {
      return FALSE;
    }
  }

  attrs = g_strsplit (val, " ", 0);
  ret = encoding_supported (media, attrs[1] /* encoding */ );
  g_strfreev (attrs);

  return ret;
}

static gboolean
add_supported_rtpmap_attrs (const GstSDPMedia * offer, GstSDPMedia * answer,
    GError ** error)
{
  guint i, len;

  len = gst_sdp_media_formats_len (answer);

  for (i = 0; i < len; i++) {
    const gchar *fmt, *val;

    fmt = gst_sdp_media_get_format (answer, i);
    val = sdp_utils_get_attr_map_value (offer, "rtpmap", fmt);

    if (val == NULL) {
      gint pt;

      /* Check if this is a static payload type so they do not need to be */
      /* set in an rtpmap attribute */

      pt = atoi (fmt);
      if (pt >= 0 && pt <= G_N_ELEMENTS (rtpmaps) && rtpmaps[pt] != NULL) {
        if (encoding_supported (offer, rtpmaps[pt])) {
          /* Static payload do not nee to be set as rtpmap attribute */
          continue;
        } else {
          GST_DEBUG ("No static payload %s supported", fmt);
          return FALSE;
        }
      } else {
        GST_DEBUG ("Not rtpmap:%s attribute found in offer", fmt);
        return FALSE;
      }
    }

    if (gst_sdp_media_add_attribute (answer, "rtpmap", val) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not add attribute rtpmap:%s", val);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
instersect_rtp_avp_media_attr (const GstSDPAttribute * attr,
    GstSDPMedia * answer, gpointer user_data)
{
  if (g_strcmp0 (attr->key, "rtpmap") == 0) {
    /* ignore */
    return TRUE;
  }

  if (gst_sdp_media_add_attribute (answer, attr->key,
          attr->value) != GST_SDP_OK) {
    GST_WARNING ("Can not add attribute %s", attr->key);
    return FALSE;
  }

  return TRUE;
}

GstSDPMedia *
kms_sdp_rtp_avp_media_handler_create_answer (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GError ** error)
{
  GstSDPMedia *m = NULL;
  gchar *proto = NULL;
  guint i, len;

  if (g_strcmp0 (gst_sdp_media_get_media (offer), SDP_AUDIO_MEDIA) != 0
      && g_strcmp0 (gst_sdp_media_get_media (offer), SDP_VIDEO_MEDIA) != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported %s media", gst_sdp_media_get_media (offer));
    goto error;
  }

  g_object_get (handler, "proto", &proto, NULL);

  if (g_strcmp0 (proto, gst_sdp_media_get_proto (offer)) != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PROTOCOL,
        "Unexpected media protocol %s", gst_sdp_media_get_proto (offer));
    goto error;
  }

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to create %s media answer", gst_sdp_media_get_media (offer));
    goto error;
  }

  if (gst_sdp_media_set_media (m,
          gst_sdp_media_get_media (offer)) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set %s media ttribute", gst_sdp_media_get_media (offer));
    goto error;
  }

  if (gst_sdp_media_set_proto (m, proto) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set proto %s attribute", proto);
    goto error;
  }

  if (gst_sdp_media_set_port_info (m, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set port attribute");
    goto error;
  }

  len = gst_sdp_media_formats_len (offer);

  /* Set only supported media formats in answer */
  for (i = 0; i < len; i++) {
    const gchar *fmt;

    fmt = gst_sdp_media_get_format (offer, i);

    if (!format_supported (offer, fmt)) {
      continue;
    }

    if (gst_sdp_media_add_format (m, fmt) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can add format %s", fmt);
      goto error;
    }
  }

  if (!add_supported_rtpmap_attrs (offer, m, error)) {
    goto error;
  }

  if (!sdp_utils_intersect_media_attributes (offer, m,
          instersect_rtp_avp_media_attr, NULL)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_UNEXPECTED_ERROR, "Can not intersect media attributes");
    goto error;
  }

  g_free (proto);

  return m;

error:
  if (proto != NULL) {
    g_free (proto);
  }

  if (m != NULL) {
    gst_sdp_media_free (m);
  }

  return NULL;
}

static void
kms_sdp_rtp_avp_media_handler_class_init (KmsSdpRtpAvpMediaHandlerClass * klass)
{
  GObjectClass *gobject_class;
  KmsSdpMediaHandlerClass *handler_class;

  gobject_class = G_OBJECT_CLASS (klass);
  handler_class = KMS_SDP_MEDIA_HANDLER_CLASS (klass);

  gobject_class->constructor = kms_sdp_rtp_avp_media_handler_constructor;
  handler_class->create_offer = kms_sdp_rtp_avp_media_handler_create_offer;
  handler_class->create_answer = kms_sdp_rtp_avp_media_handler_create_answer;
}

static void
kms_sdp_rtp_avp_media_handler_init (KmsSdpRtpAvpMediaHandler * self)
{
  /* Nothing to do here */
}

KmsSdpRtpAvpMediaHandler *
kms_sdp_rtp_avp_media_handler_new (void)
{
  KmsSdpRtpAvpMediaHandler *handler;

  handler =
      KMS_SDP_RTP_AVP_MEDIA_HANDLER (g_object_new
      (KMS_TYPE_SDP_RTP_AVP_MEDIA_HANDLER, NULL));

  return handler;
}
