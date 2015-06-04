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

#include <uuid/uuid.h>
#include <stdlib.h>

#include "kms-core-enumtypes.h"
#include "kms-core-marshal.h"
#include "sdp_utils.h"
#include "sdpagent/kmssdprtpavpfmediahandler.h"
#include "kmsremb.h"
#include "kmsistats.h"

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/video/video-event.h>

#define PLUGIN_NAME "base_rtp_endpoint"

GST_DEBUG_CATEGORY_STATIC (kms_base_rtp_endpoint_debug);
#define GST_CAT_DEFAULT kms_base_rtp_endpoint_debug

#define kms_base_rtp_endpoint_parent_class parent_class
#define UUID_STR_SIZE 37        /* 36-byte string (plus tailing '\0') */
#define KMS_KEY_ID "kms-key-id"

static void kms_istats_interface_init (KmsIStatsInterface * iface);

G_DEFINE_TYPE_WITH_CODE (KmsBaseRtpEndpoint, kms_base_rtp_endpoint,
    KMS_TYPE_BASE_SDP_ENDPOINT,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_ISTATS, kms_istats_interface_init);
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME));

#define KMS_BASE_RTP_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                   \
    (obj),                                        \
    KMS_TYPE_BASE_RTP_ENDPOINT,                   \
    KmsBaseRtpEndpointPrivate                     \
  )                                               \
)

#define RTCP_DEMUX_PEER "rtcp-demux-peer"

#define RTP_HDR_EXT_ABS_SEND_TIME_URI "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"
#define RTP_HDR_EXT_ABS_SEND_TIME_SIZE 3
#define RTP_HDR_EXT_ABS_SEND_TIME_ID 3  /* TODO: do it dynamic when needed */

#define JB_INITIAL_LATENCY 0
#define JB_READY_LATENCY 1500

typedef struct _KmsSSRCStats KmsSSRCStats;
struct _KmsSSRCStats
{
  guint ssrc;
  GstElement *jitter_buffer;
};

typedef struct _KmsRTPSessionStats KmsRTPSessionStats;
struct _KmsRTPSessionStats
{
  GObject *rtp_session;
  GSList *ssrcs;                /* list of all jitter buffers associated to a ssrc */
};

struct _KmsBaseRtpEndpointPrivate
{
  GstElement *rtpbin;
  KmsMediaState state;
  gboolean audio_actived;
  gboolean video_actived;

  gboolean rtcp_mux;
  gboolean rtcp_nack;
  gboolean rtcp_remb;

  GHashTable *conns;

  GstElement *audio_payloader;
  GstElement *video_payloader;

  gboolean audio_payloader_connected;
  gboolean video_payloader_connected;

  guint local_audio_ssrc;
  guint remote_audio_ssrc;
  guint audio_ssrc;

  guint local_video_ssrc;
  guint remote_video_ssrc;
  guint video_ssrc;

  gint32 target_bitrate;
  guint min_video_send_bw;
  guint max_video_send_bw;

  /* REMB */
  KmsRembLocal *rl;
  KmsRembRemote *rm;

  /* RTP statistics */
  GHashTable *stats;
};

/* Signals and args */
enum
{
  MEDIA_START,
  MEDIA_STOP,
  MEDIA_STATE_CHANGED,
  SIGNAL_REQUEST_LOCAL_KEY_FRAME,
  LAST_SIGNAL
};

static guint obj_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_RTCP_MUX    FALSE
#define DEFAULT_RTCP_NACK    FALSE
#define DEFAULT_RTCP_REMB    FALSE
#define DEFAULT_TARGET_BITRATE    0
#define MIN_VIDEO_SEND_BW_DEFAULT 100
#define MAX_VIDEO_SEND_BW_DEFAULT 500

enum
{
  PROP_0,
  PROP_RTCP_MUX,
  PROP_RTCP_NACK,
  PROP_RTCP_REMB,
  PROP_TARGET_BITRATE,
  PROP_MIN_VIDEO_SEND_BW,
  PROP_MAX_VIDEO_SEND_BW,
  PROP_STATE,
  PROP_LAST
};

/* Media handler management begin */
static void
kms_base_rtp_create_media_handler (KmsBaseSdpEndpoint * base_sdp,
    KmsSdpMediaHandler ** handler)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp);
  KmsSdpRtpAvpMediaHandler *h_avp;
  GError *err = NULL;

  if (!KMS_IS_SDP_RTP_AVP_MEDIA_HANDLER (*handler)) {
    GST_WARNING_OBJECT (base_sdp, "Handler should be or extend from RTP/AVP.");
    return;
  }

  g_object_set (G_OBJECT (*handler), "rtcp-mux", self->priv->rtcp_mux, NULL);

  if (KMS_IS_SDP_RTP_AVPF_MEDIA_HANDLER (*handler)) {
    g_object_set (G_OBJECT (*handler), "nack", self->priv->rtcp_nack,
        "goog-remb", self->priv->rtcp_remb, NULL);
  }

  h_avp = KMS_SDP_RTP_AVP_MEDIA_HANDLER (*handler);
  kms_sdp_rtp_avp_media_handler_add_extmap (h_avp, RTP_HDR_EXT_ABS_SEND_TIME_ID,
      RTP_HDR_EXT_ABS_SEND_TIME_URI, &err);
  if (err != NULL) {
    GST_WARNING_OBJECT (base_sdp, "Cannot add extmap '%s'", err->message);
  }
}

/* Media handler management end */

typedef struct _ConnectPayloaderData
{
  KmsBaseRtpEndpoint *self;
  GstElement *payloader;
  gboolean *connected_flag;
  KmsElementPadType type;
} ConnectPayloaderData;

static ConnectPayloaderData *
connect_payloader_data_new (KmsBaseRtpEndpoint * self, GstElement * payloader,
    gboolean * connected_flag, KmsElementPadType type)
{
  ConnectPayloaderData *data;

  data = g_slice_new0 (ConnectPayloaderData);

  data->self = self;
  data->payloader = payloader;
  data->connected_flag = connected_flag;
  data->type = type;

  return data;
}

static void
connect_payloader_data_destroy (gpointer data, GClosure * closure)
{
  g_slice_free (ConnectPayloaderData, data);
}

static KmsSSRCStats *
ssrc_stats_new (guint ssrc, GstElement * jitter_buffer)
{
  KmsSSRCStats *stats;

  stats = g_slice_new0 (KmsSSRCStats);

  stats->jitter_buffer = gst_object_ref (jitter_buffer);
  stats->ssrc = ssrc;

  return stats;
}

static void
ssrc_stats_destroy (KmsSSRCStats * stats)
{
  g_clear_object (&stats->jitter_buffer);
  g_slice_free (KmsSSRCStats, stats);
}

static KmsRTPSessionStats *
rtp_session_stats_new (GObject * rtp_session)
{
  KmsRTPSessionStats *stats;

  stats = g_slice_new0 (KmsRTPSessionStats);
  stats->rtp_session = g_object_ref (rtp_session);

  return stats;
}

static void
rtp_session_stats_destroy (KmsRTPSessionStats * stats)
{
  if (stats->ssrcs != NULL) {
    g_slist_free_full (stats->ssrcs, (GDestroyNotify) ssrc_stats_destroy);
  }

  g_clear_object (&stats->rtp_session);

  g_slice_free (KmsRTPSessionStats, stats);
}

static gboolean
rtcp_fb_attr_check_type (const gchar * attr, const gchar * pt,
    const gchar * type)
{
  gchar *aux;
  gboolean ret;

  aux = g_strconcat (pt, " ", type, NULL);
  ret = g_strcmp0 (attr, aux) == 0;
  g_free (aux);

  return ret;
}

static gboolean
media_has_remb (const GstSDPMedia * media)
{
  const gchar *payload = gst_sdp_media_get_format (media, 0);
  guint a;

  if (payload == NULL) {
    return FALSE;
  }

  for (a = 0;; a++) {
    const gchar *attr;

    attr = gst_sdp_media_get_attribute_val_n (media, RTCP_FB, a);
    if (attr == NULL) {
      break;
    }

    if (rtcp_fb_attr_check_type (attr, payload, RTCP_FB_REMB)) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
media_has_rtcp_nack (const GstSDPMedia * media)
{
  const gchar *payload = gst_sdp_media_get_format (media, 0);
  guint a;

  if (payload == NULL) {
    return FALSE;
  }

  for (a = 0;; a++) {
    const gchar *attr;

    attr = gst_sdp_media_get_attribute_val_n (media, RTCP_FB, a);
    if (attr == NULL) {
      break;
    }

    if (rtcp_fb_attr_check_type (attr, payload, RTCP_FB_NACK)) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
kms_base_rtp_endpoint_is_video_rtcp_nack (KmsBaseRtpEndpoint * self)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *negotiated_ctx;
  const GSList *item;

  negotiated_ctx = kms_base_sdp_endpoint_get_negotiated_sdp_ctx (base_endpoint);
  if (negotiated_ctx == NULL) {
    GST_WARNING_OBJECT (self, "Negotiation ctx not set");
    return FALSE;
  }

  item = kms_sdp_message_context_get_medias (negotiated_ctx);
  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
    const gchar *media_str = gst_sdp_media_get_media (media);

    if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
      return media_has_rtcp_nack (media);
    }
  }

  return FALSE;
}

/* Connection management begin */

static KmsIRtpConnection *
kms_base_rtp_endpoint_create_connection_default (KmsBaseRtpEndpoint * self,
    const gchar * name)
{
  KmsBaseRtpEndpointClass *klass =
      KMS_BASE_RTP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->create_connection ==
      kms_base_rtp_endpoint_create_connection_default) {
    GST_WARNING_OBJECT (self, "%s does not reimplement 'create_connection'",
        G_OBJECT_CLASS_NAME (klass));
  }

  return NULL;
}

static KmsIRtcpMuxConnection *
kms_base_rtp_endpoint_create_rtcp_mux_connection_default (KmsBaseRtpEndpoint *
    self, const gchar * name)
{
  KmsBaseRtpEndpointClass *klass =
      KMS_BASE_RTP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->create_rtcp_mux_connection ==
      kms_base_rtp_endpoint_create_rtcp_mux_connection_default) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement 'create_rtcp_mux_connection'",
        G_OBJECT_CLASS_NAME (klass));
  }

  return NULL;
}

static KmsIBundleConnection *
kms_base_rtp_endpoint_create_bundle_connection_default (KmsBaseRtpEndpoint *
    self, const gchar * name)
{
  KmsBaseRtpEndpointClass *klass =
      KMS_BASE_RTP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->create_bundle_connection ==
      kms_base_rtp_endpoint_create_bundle_connection_default) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement 'create_bundle_connection'",
        G_OBJECT_CLASS_NAME (klass));
  }

  return NULL;
}

static gchar *
create_connection_name_from_media_config (SdpMediaConfig * mconf)
{
  SdpMediaGroup *group = kms_sdp_media_config_get_group (mconf);
  gchar *conn_name;

  if (group != NULL) {
    gint gid = kms_sdp_media_group_get_id (group);

    conn_name =
        g_strdup_printf ("%s%" G_GINT32_FORMAT, BUNDLE_STREAM_NAME, gid);
  } else {
    gint mid = kms_sdp_media_config_get_id (mconf);

    conn_name = g_strdup_printf ("%" G_GINT32_FORMAT, mid);
  }

  return conn_name;
}

static KmsIRtpConnection *
kms_base_rtp_endpoint_get_connection_by_name (KmsBaseRtpEndpoint * self,
    const gchar * name)
{
  gpointer *conn;

  KMS_ELEMENT_LOCK (self);
  conn = g_hash_table_lookup (self->priv->conns, name);
  KMS_ELEMENT_UNLOCK (self);

  if (conn == NULL) {
    return NULL;
  }

  return KMS_I_RTP_CONNECTION (conn);
}

KmsIRtpConnection *
kms_base_rtp_endpoint_get_connection (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf)
{
  gchar *name = create_connection_name_from_media_config (mconf);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_endpoint_get_connection_by_name (self, name);
  if (conn == NULL) {
    GST_WARNING_OBJECT (self, "Connection '%s' not found", name);
    g_free (name);
    return NULL;
  }
  g_free (name);

  return conn;
}

GHashTable *
kms_base_rtp_endpoint_get_connections (KmsBaseRtpEndpoint * self)
{
  return self->priv->conns;
}

static KmsIRtpConnection *
kms_base_rtp_endpoint_create_connection (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpointClass *base_rtp_class;
  gchar *name = create_connection_name_from_media_config (mconf);
  SdpMediaGroup *group = kms_sdp_media_config_get_group (mconf);
  KmsIRtpConnection *conn;

  g_return_val_if_fail (KMS_IS_BASE_RTP_ENDPOINT (self), NULL);

  conn = kms_base_rtp_endpoint_get_connection_by_name (self, name);
  if (conn != NULL) {
    GST_WARNING_OBJECT (self, "Connection '%s' already created", name);
    goto end;
  }

  base_rtp_class = KMS_BASE_RTP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (group != NULL) {          /* bundle */
    conn =
        KMS_I_RTP_CONNECTION (base_rtp_class->create_bundle_connection (self,
            name));
  } else {
    if (kms_sdp_media_config_is_rtcp_mux (mconf)) {
      conn =
          KMS_I_RTP_CONNECTION (base_rtp_class->create_rtcp_mux_connection
          (self, name));
    } else {
      conn = base_rtp_class->create_connection (self, name);
    }
  }

  g_hash_table_insert (self->priv->conns, g_strdup (name), conn);

end:
  g_free (name);

  return conn;
}

/* Connection management end */

/* Configure media SDP begin */
static void
assign_uuid (GObject * ssrc)
{
  gchar *uuid_str;
  uuid_t uuid;

  uuid_str = (gchar *) g_malloc0 (UUID_STR_SIZE);
  uuid_generate (uuid);
  uuid_unparse (uuid, uuid_str);

  /* Assign a unique ID to each SSRC which will */
  /* be provided in statistics */
  g_object_set_data_full (ssrc, KMS_KEY_ID, uuid_str, g_free);
}

static GObject *
kms_base_rtp_endpoint_create_rtp_session (KmsBaseRtpEndpoint * self,
    guint session_id, const gchar * rtpbin_pad_name)
{
  GstElement *rtpbin = self->priv->rtpbin;
  KmsRTPSessionStats *rtp_stats;
  GObject *rtpsession;
  GstPad *pad;

  /* Create RtpSession requesting the pad */
  pad = gst_element_get_request_pad (rtpbin, rtpbin_pad_name);
  g_object_unref (pad);

  g_signal_emit_by_name (rtpbin, "get-internal-session", session_id,
      &rtpsession);
  if (rtpsession == NULL) {
    return NULL;
  }

  rtp_stats =
      g_hash_table_lookup (self->priv->stats, GUINT_TO_POINTER (session_id));

  if (rtp_stats == NULL) {
    rtp_stats = rtp_session_stats_new (rtpsession);
    g_hash_table_insert (self->priv->stats, GUINT_TO_POINTER (session_id),
        rtp_stats);
  } else {
    GST_WARNING_OBJECT (self, "Session %u already created", session_id);
  }

  g_object_set (rtpsession, "rtcp-min-interval",
      RTCP_MIN_INTERVAL * GST_MSECOND, NULL);

  return rtpsession;
}

static gboolean
kms_base_rtp_endpoint_configure_rtp_media (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *proto_str = gst_sdp_media_get_proto (media);
  const gchar *media_str = gst_sdp_media_get_media (media);
  const gchar *rtpbin_pad_name = NULL;
  guint session_id;
  GObject *rtpsession;
  GstStructure *sdes = NULL;
  const gchar *cname;
  guint ssrc;
  gchar *str;

  if (!g_str_has_prefix (proto_str, "RTP")) {
    GST_DEBUG_OBJECT (self, "'%s' protocol not need RTP session", proto_str);
    return TRUE;
  }

  /* TODO: think about this when multiple audio/video medias */
  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    rtpbin_pad_name = AUDIO_RTPBIN_SEND_RTP_SINK;
    session_id = AUDIO_RTP_SESSION;
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    rtpbin_pad_name = VIDEO_RTPBIN_SEND_RTP_SINK;
    session_id = VIDEO_RTP_SESSION;
  } else {
    GST_WARNING_OBJECT (self, "Media '%s' not supported", media_str);
    return FALSE;
  }

  rtpsession =
      kms_base_rtp_endpoint_create_rtp_session (self, session_id,
      rtpbin_pad_name);
  if (rtpsession == NULL) {
    GST_WARNING_OBJECT (self,
        "Cannot create RTP Session'%" G_GUINT32_FORMAT "'", session_id);
    return FALSE;
  }

  g_object_get (self->priv->rtpbin, "sdes", &sdes, NULL);
  cname = gst_structure_get_string (sdes, "cname");
  g_object_get (rtpsession, "internal-ssrc", &ssrc, NULL);
  g_object_unref (rtpsession);

  str = g_strdup_printf ("%" G_GUINT32_FORMAT " cname:%s", ssrc, cname);
  gst_sdp_media_add_attribute (media, "ssrc", str);
  g_free (str);
  gst_structure_free (sdes);

  if (session_id == AUDIO_RTP_SESSION) {
    self->priv->local_audio_ssrc = ssrc;
  } else if (session_id == VIDEO_RTP_SESSION) {
    self->priv->local_video_ssrc = ssrc;
  }

  return TRUE;
}

static gboolean
kms_base_rtp_endpoint_configure_media (KmsBaseSdpEndpoint *
    base_sdp_endpoint, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_endpoint_create_connection (self, mconf);
  if (conn == NULL) {
    return FALSE;
  }

  return kms_base_rtp_endpoint_configure_rtp_media (self, mconf);
}

/* Configure media SDP end */

/* Start Transport Send begin */

static void
kms_base_rtp_endpoint_create_remb_managers (KmsBaseRtpEndpoint * self)
{
  GstElement *rtpbin = self->priv->rtpbin;
  GObject *rtpsession;
  GstPad *pad;
  int max_recv_bw;

  if (self->priv->rl != NULL) {
    /* TODO: support more than one media with REMB */
    GST_WARNING_OBJECT (self, "Only support for one media with REMB");
    return;
  }

  g_signal_emit_by_name (rtpbin, "get-internal-session", VIDEO_RTP_SESSION,
      &rtpsession);
  if (rtpsession == NULL) {
    GST_WARNING_OBJECT (self,
        "There is not session with id %" G_GUINT32_FORMAT, VIDEO_RTP_SESSION);
    return;
  }

  g_object_get (self, "max-video-recv-bandwidth", &max_recv_bw, NULL);
  self->priv->rl =
      kms_remb_local_create (rtpsession, VIDEO_RTP_SESSION,
      self->priv->remote_video_ssrc, max_recv_bw);

  pad = gst_element_get_static_pad (rtpbin, VIDEO_RTPBIN_SEND_RTP_SINK);
  self->priv->rm =
      kms_remb_remote_create (rtpsession, VIDEO_RTP_SESSION,
      self->priv->local_video_ssrc, self->priv->min_video_send_bw,
      self->priv->max_video_send_bw, pad);
  g_object_unref (pad);
  g_object_unref (rtpsession);

  GST_DEBUG_OBJECT (self, "REMB managers added");
}

static gboolean
ssrcs_are_mapped (GstElement * ssrcdemux,
    guint32 local_ssrc, guint32 remote_ssrc)
{
  GstElement *rtcpdemux =
      g_object_get_data (G_OBJECT (ssrcdemux), RTCP_DEMUX_PEER);
  guint local_ssrc_pair;

  g_signal_emit_by_name (rtcpdemux, "get-local-rr-ssrc-pair", remote_ssrc,
      &local_ssrc_pair);

  return ((local_ssrc != 0) && (local_ssrc_pair == local_ssrc));
}

static void
rtp_ssrc_demux_new_ssrc_pad (GstElement * ssrcdemux, guint ssrc, GstPad * pad,
    KmsBaseRtpEndpoint * self)
{
  const gchar *rtp_pad_name = GST_OBJECT_NAME (pad);
  gchar *rtcp_pad_name;
  GstElement *rtpbin = self->priv->rtpbin;

  GST_DEBUG_OBJECT (self, "pad: %" GST_PTR_FORMAT " ssrc: %" G_GUINT32_FORMAT,
      pad, ssrc);
  rtcp_pad_name = g_strconcat ("rtcp_", rtp_pad_name, NULL);

  KMS_ELEMENT_LOCK (self);

  if ((self->priv->remote_audio_ssrc == ssrc) ||
      ssrcs_are_mapped (ssrcdemux, self->priv->local_audio_ssrc, ssrc)) {
    gst_element_link_pads (ssrcdemux, rtp_pad_name, rtpbin,
        AUDIO_RTPBIN_RECV_RTP_SINK);
    gst_element_link_pads (ssrcdemux, rtcp_pad_name, rtpbin,
        AUDIO_RTPBIN_RECV_RTCP_SINK);
  } else if (self->priv->remote_video_ssrc == ssrc
      || ssrcs_are_mapped (ssrcdemux, self->priv->local_video_ssrc, ssrc)) {
    gst_element_link_pads (ssrcdemux, rtp_pad_name, rtpbin,
        VIDEO_RTPBIN_RECV_RTP_SINK);
    gst_element_link_pads (ssrcdemux, rtcp_pad_name, rtpbin,
        VIDEO_RTPBIN_RECV_RTCP_SINK);
  }

  KMS_ELEMENT_UNLOCK (self);

  g_free (rtcp_pad_name);
}

static void
kms_base_rtp_endpoint_add_bundle_connection (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, gboolean active)
{
  gboolean connected;
  GstElement *ssrcdemux;
  GstElement *rtcpdemux;        /* FIXME: Useful for local and remote ssrcs mapping */
  GstPad *src, *sink;

  g_object_get (conn, "added", &connected, NULL);
  if (connected) {
    GST_DEBUG_OBJECT (self, "Conn already added");
    return;
  }

  ssrcdemux = gst_element_factory_make ("rtpssrcdemux", NULL);
  rtcpdemux = gst_element_factory_make ("rtcpdemux", NULL);

  g_object_set_data_full (G_OBJECT (ssrcdemux), RTCP_DEMUX_PEER,
      g_object_ref (rtcpdemux), g_object_unref);
  g_signal_connect (ssrcdemux, "new-ssrc-pad",
      G_CALLBACK (rtp_ssrc_demux_new_ssrc_pad), self);

  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);
  gst_bin_add_many (GST_BIN (self), ssrcdemux, rtcpdemux, NULL);

  /* RTP */
  src = kms_i_rtp_connection_request_rtp_src (conn);
  sink = gst_element_get_static_pad (ssrcdemux, "sink");
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  src = kms_i_rtp_connection_request_rtcp_src (conn);
  sink = gst_element_get_static_pad (rtcpdemux, "sink");
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
  gst_element_link_pads (rtcpdemux, "rtcp_src", ssrcdemux, "rtcp_sink");

  gst_element_sync_state_with_parent_target_state (ssrcdemux);
  gst_element_sync_state_with_parent_target_state (rtcpdemux);
  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

static void
kms_base_rtp_endpoint_write_rtp_hdr_ext (GstPad * pad, GstBuffer * buffer,
    gint id)
{
  GstRTPBuffer rtp = { NULL, };
  guint8 *data;
  GstClockTime current_time, ms;
  guint value;

  if (!gst_rtp_buffer_map (buffer, GST_MAP_WRITE, &rtp)) {
    GST_WARNING_OBJECT (pad, "Can not map RTP buffer for writting");
    return;
  }

  current_time = kms_utils_get_time_nsecs ();
  ms = current_time / 1000000;
  value = (((ms << 18) / 1000) & 0x00ffffff);

  data = g_malloc0 (RTP_HDR_EXT_ABS_SEND_TIME_SIZE);
  data[0] = (guint8) (value >> 16);
  data[1] = (guint8) (value >> 8);
  data[2] = (guint8) (value);

  /* TODO: check if header exists and in this case replace the value */
  if (!gst_rtp_buffer_add_extension_onebyte_header (&rtp,
          id, data, RTP_HDR_EXT_ABS_SEND_TIME_SIZE)) {
    GST_WARNING_OBJECT (pad, "RTP hdrext abs-send-time not added");
  }

  g_free (data);
  gst_rtp_buffer_unmap (&rtp);
}

typedef struct _HdrExtData
{
  GstPad *pad;
  gint abs_send_time_id;
} HdrExtData;

static gboolean
kms_base_rtp_endpoint_write_rtp_hdr_ext_bufflist (GstBuffer ** buf, guint idx,
    HdrExtData * data)
{
  *buf = gst_buffer_make_writable (*buf);
  kms_base_rtp_endpoint_write_rtp_hdr_ext (data->pad, *buf,
      data->abs_send_time_id);

  return TRUE;
}

static GstPadProbeReturn
kms_base_rtp_endpoint_write_rtp_hdr_ext_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer gp)
{
  gint id = GPOINTER_TO_INT (gp);

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

    buffer = gst_buffer_make_writable (buffer);
    kms_base_rtp_endpoint_write_rtp_hdr_ext (pad, buffer, id);
    GST_PAD_PROBE_INFO_DATA (info) = buffer;
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *bufflist = GST_PAD_PROBE_INFO_BUFFER_LIST (info);
    HdrExtData data;

    data.pad = pad;
    data.abs_send_time_id = id;

    bufflist = gst_buffer_list_make_writable (bufflist);
    gst_buffer_list_foreach (bufflist,
        (GstBufferListFunc) kms_base_rtp_endpoint_write_rtp_hdr_ext_bufflist,
        &data);

    GST_PAD_PROBE_INFO_DATA (info) = bufflist;
  }

  return GST_PAD_PROBE_OK;
}

static void
kms_base_rtp_endpoint_add_connection_sink (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, const gchar * rtp_session, gint abs_send_time_id)
{
  GstPad *src, *sink;
  gchar *str;

  str = g_strdup_printf ("%s%s", RTPBIN_SEND_RTP_SRC, rtp_session);
  src = gst_element_get_static_pad (self->priv->rtpbin, str);
  g_free (str);
  sink = kms_i_rtp_connection_request_rtp_sink (conn);
  gst_pad_link (src, sink);

  if (abs_send_time_id > -1) {
    GST_DEBUG_OBJECT (self,
        "Add probe for abs-send-time management (id: %d, %" GST_PTR_FORMAT ").",
        abs_send_time_id, src);
    gst_pad_add_probe (src,
        GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
        kms_base_rtp_endpoint_write_rtp_hdr_ext_probe,
        GINT_TO_POINTER (abs_send_time_id), NULL);
  }

  g_object_unref (src);
  g_object_unref (sink);

  str = g_strdup_printf ("%s%s", RTPBIN_SEND_RTCP_SRC, rtp_session);
  src = gst_element_get_request_pad (self->priv->rtpbin, str);
  g_free (str);
  sink = kms_i_rtp_connection_request_rtcp_sink (conn);
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
}

static void
kms_base_rtp_endpoint_add_connection_src (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, const gchar * rtp_session)
{
  GstPad *src, *sink;
  gchar *str;

  str = g_strdup_printf ("%s%s", RTPBIN_RECV_RTP_SINK, rtp_session);
  src = kms_i_rtp_connection_request_rtp_src (conn);
  sink = gst_element_get_request_pad (self->priv->rtpbin, str);
  g_free (str);
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  str = g_strdup_printf ("%s%s", RTPBIN_RECV_RTCP_SINK, rtp_session);
  src = kms_i_rtp_connection_request_rtcp_src (conn);
  sink = gst_element_get_request_pad (self->priv->rtpbin, str);
  g_free (str);
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
}

static void
kms_base_rtp_endpoint_add_rtcp_mux_connection (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, gboolean active, const gchar * rtp_session,
    gint abs_send_time_id)
{
  /* FIXME: Useful for local and remote ssrcs mapping */
  GstElement *rtcpdemux = gst_element_factory_make ("rtcpdemux", NULL);
  GstPad *src, *sink;
  gchar *str;

  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);
  gst_bin_add (GST_BIN (self), rtcpdemux);

  /* RTP */
  src = kms_i_rtp_connection_request_rtp_src (conn);
  str = g_strdup_printf ("%s%s", RTPBIN_RECV_RTP_SINK, rtp_session);
  sink = gst_element_get_static_pad (self->priv->rtpbin, str);
  if (!sink)
    sink = gst_element_get_request_pad (self->priv->rtpbin, str);
  g_free (str);
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  src = kms_i_rtp_connection_request_rtcp_src (conn);
  sink = gst_element_get_static_pad (rtcpdemux, "sink");
  gst_pad_link (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
  str = g_strdup_printf ("%s%s", RTPBIN_RECV_RTCP_SINK, rtp_session);
  gst_element_link_pads (rtcpdemux, "rtcp_src", self->priv->rtpbin, str);
  g_free (str);

  gst_element_sync_state_with_parent_target_state (rtcpdemux);
  kms_base_rtp_endpoint_add_connection_sink (self, conn, rtp_session,
      abs_send_time_id);

  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

static void
kms_base_rtp_endpoint_add_connection (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, gboolean active, const gchar * rtp_session,
    gint abs_send_time_id)
{
  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);

  kms_base_rtp_endpoint_add_connection_sink (self, conn, rtp_session,
      abs_send_time_id);
  kms_base_rtp_endpoint_add_connection_src (self, conn, rtp_session);

  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

static gint
get_abs_send_time_id (SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  guint a;

  for (a = 0;; a++) {
    const gchar *attr;
    gchar **tokens;

    attr = gst_sdp_media_get_attribute_val_n (media, EXT_MAP, a);
    if (attr == NULL) {
      break;
    }

    tokens = g_strsplit (attr, " ", 0);
    if (g_strcmp0 (RTP_HDR_EXT_ABS_SEND_TIME_URI, tokens[1]) == 0) {
      gint ret = atoi (tokens[0]);

      g_strfreev (tokens);
      return ret;
    }

    g_strfreev (tokens);
  }

  return -1;
}

static gboolean
kms_base_rtp_endpoint_add_connection_for_session (KmsBaseRtpEndpoint * self,
    const gchar * rtp_session, SdpMediaConfig * mconf, gboolean active)
{
  KmsIRtpConnection *conn;
  SdpMediaGroup *group = kms_sdp_media_config_get_group (mconf);
  gint abs_send_time_id;

  conn = kms_base_rtp_endpoint_get_connection (self, mconf);
  if (conn == NULL) {
    return FALSE;
  }

  abs_send_time_id = get_abs_send_time_id (mconf);

  if (group != NULL) {          /* bundle */
    kms_base_rtp_endpoint_add_bundle_connection (self, conn, active);
    kms_base_rtp_endpoint_add_connection_sink (self, conn, rtp_session,
        abs_send_time_id);
  } else if (kms_sdp_media_config_is_rtcp_mux (mconf)) {
    kms_base_rtp_endpoint_add_rtcp_mux_connection (self, conn, active,
        rtp_session, abs_send_time_id);
  } else {
    kms_base_rtp_endpoint_add_connection (self, conn, active, rtp_session,
        abs_send_time_id);
  }

  return TRUE;
}

static gboolean
kms_base_rtp_endpoint_sdp_media_is_active (KmsBaseRtpEndpoint * self,
    GstSDPMedia * media, gboolean offerer)
{
  const gchar *attr;

  attr = gst_sdp_media_get_attribute_val_n (media, "setup", 0);
  if (attr == NULL) {
    goto _default;
  }

  if (offerer) {
    if (g_strcmp0 (attr, "active") == 0) {
      GST_DEBUG_OBJECT (self, "Remote is 'active', so we are 'passive'");
      return FALSE;
    } else if (g_strcmp0 (attr, "passive") == 0) {
      GST_DEBUG_OBJECT (self, "Remote is 'passive', so we are 'active'");
      return TRUE;
    }
  } else {
    if (g_strcmp0 (attr, "active") == 0) {
      GST_DEBUG_OBJECT (self, "We are 'active'");
      return TRUE;
    } else if (g_strcmp0 (attr, "passive") == 0) {
      GST_DEBUG_OBJECT (self, "We are 'passive'");
      return FALSE;
    }
  }

_default:
  GST_DEBUG_OBJECT (self, "Negotiated SDP is '%s'. %s", attr,
      offerer ? "Local offerer, so 'passive'" : "Remote offerer, so 'active'");

  return !offerer;
}

static const gchar *
kms_base_rtp_endpoint_process_remote_ssrc (KmsBaseRtpEndpoint * self,
    GstSDPMedia * remote_media)
{
  const gchar *media_str = gst_sdp_media_get_media (remote_media);

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    /* TODO: support more than one in the future */
    if (self->priv->remote_audio_ssrc != 0) {
      GST_WARNING_OBJECT (self,
          "Overwriting remote audio ssrc. This can cause some problem");
    }
    self->priv->remote_audio_ssrc = sdp_utils_media_get_ssrc (remote_media);
    return AUDIO_RTP_SESSION_STR;
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    /* TODO: support more than one in the future */
    if (self->priv->remote_video_ssrc != 0) {
      GST_WARNING_OBJECT (self,
          "Overwriting remote video ssrc. This can cause some problem");
    }
    self->priv->remote_video_ssrc = sdp_utils_media_get_ssrc (remote_media);
    return VIDEO_RTP_SESSION_STR;
  }

  GST_WARNING_OBJECT (self, "Media '%s' not supported", media_str);

  return NULL;
}

static gboolean
kms_base_rtp_endpoint_configure_connection (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * neg_mconf, SdpMediaConfig * remote_mconf, gboolean offerer)
{
  GstSDPMedia *neg_media = kms_sdp_media_config_get_sdp_media (neg_mconf);
  const gchar *neg_proto_str = gst_sdp_media_get_proto (neg_media);
  const gchar *neg_media_str = gst_sdp_media_get_media (neg_media);

  GstSDPMedia *remote_media = kms_sdp_media_config_get_sdp_media (remote_mconf);
  const gchar *remote_proto_str = gst_sdp_media_get_proto (remote_media);
  const gchar *remote_media_str = gst_sdp_media_get_media (remote_media);

  const gchar *rtp_session_str;
  gboolean active;

  if (g_strcmp0 (neg_proto_str, remote_proto_str) != 0) {
    GST_WARNING_OBJECT (self,
        "Negotiated proto ('%s') not matching with remote proto ('%s')",
        neg_proto_str, remote_proto_str);
    return FALSE;
  }

  if (!g_str_has_prefix (neg_proto_str, "RTP")) {
    GST_DEBUG_OBJECT (self, "'%s' protocol not need RTP connection",
        neg_proto_str);
    return TRUE;                /* It cannot be managed here but could be managed by the child class */
  }

  if (g_strcmp0 (neg_media_str, remote_media_str) != 0) {
    GST_WARNING_OBJECT (self,
        "Negotiated media ('%s') not matching with remote media ('%s')",
        neg_media_str, remote_media_str);
    return FALSE;
  }

  rtp_session_str =
      kms_base_rtp_endpoint_process_remote_ssrc (self, remote_media);
  if (rtp_session_str == NULL) {
    return TRUE;                /* It cannot be managed here but could be managed by the child class */
  }

  if (media_has_remb (neg_media)) {
    kms_base_rtp_endpoint_create_remb_managers (self);
  }

  active = kms_base_rtp_endpoint_sdp_media_is_active (self, neg_media, offerer);

  return kms_base_rtp_endpoint_add_connection_for_session (self,
      rtp_session_str, neg_mconf, active);
}

static void
kms_base_rtp_endpoint_start_transport_send (KmsBaseSdpEndpoint *
    base_sdp_endpoint, gboolean offerer)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  SdpMessageContext *neg_ctx =
      kms_base_sdp_endpoint_get_negotiated_sdp_ctx (base_sdp_endpoint);
  SdpMessageContext *remote_ctx =
      kms_base_sdp_endpoint_get_remote_sdp_ctx (base_sdp_endpoint);
  GSList *item = kms_sdp_message_context_get_medias (neg_ctx);
  GSList *remote_media_list = kms_sdp_message_context_get_medias (remote_ctx);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *neg_mconf = item->data;
    gint mid = kms_sdp_media_config_get_id (neg_mconf);
    SdpMediaConfig *remote_mconf;

    if (kms_sdp_media_config_is_inactive (neg_mconf)) {
      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    remote_mconf = g_slist_nth_data (remote_media_list, mid);
    if (remote_mconf == NULL) {
      GST_WARNING_OBJECT (self, "Media (id=%d) is not in the remote SDP", mid);
      continue;
    }

    if (!kms_base_rtp_endpoint_configure_connection (self, neg_mconf,
            remote_mconf, offerer)) {
      GST_WARNING_OBJECT (self, "Cannot configure connection.");
    }
  }
}

/* Start Transport Send end */

static gboolean
kms_base_rtp_endpoint_request_local_key_frame (KmsBaseRtpEndpoint * self)
{
  GstPad *pad;
  GstEvent *event;
  gboolean ret;

  GST_TRACE_OBJECT (self, "Request local key frame.");

  pad =
      gst_element_get_static_pad (self->priv->rtpbin,
      VIDEO_RTPBIN_SEND_RTP_SRC);
  if (pad == NULL) {
    GST_WARNING_OBJECT (self, "Not configured to request local key frame.");
    return FALSE;
  }

  event =
      gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
      TRUE, 0);
  ret = gst_pad_send_event (pad, event);
  g_object_unref (pad);

  if (ret == FALSE) {
    GST_WARNING_OBJECT (self, "Key frame request not handled");
  }

  return ret;
}

/* Connect input elements begin */
/* Payloading configuration begin */
static const gchar *
get_caps_codec_name (const gchar * codec_name)
{
  if (g_ascii_strcasecmp (OPUS_ENCONDING_NAME, codec_name) == 0) {
    return "X-GST-OPUS-DRAFT-SPITTKA-00";
  }
  if (g_ascii_strcasecmp (VP8_ENCONDING_NAME, codec_name) == 0) {
    return "VP8-DRAFT-IETF-01";
  }

  return codec_name;
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
kms_base_rtp_endpoint_do_connect_payloader (KmsBaseRtpEndpoint * self,
    GstElement * payloader, gboolean * connected_flag, KmsElementPadType type)
{
  GST_DEBUG_OBJECT (self, "Connecting payloader %" GST_PTR_FORMAT, payloader);

  if (g_atomic_int_compare_and_exchange (connected_flag, FALSE, TRUE)) {
    GstPad *target = gst_element_get_static_pad (payloader, "sink");

    kms_element_connect_sink_target (KMS_ELEMENT (self), target, type);
    g_object_unref (target);
  } else {
    GST_WARNING_OBJECT (self,
        "Connected flag already set for payloader %" GST_PTR_FORMAT, payloader);
  }
}

static void
kms_base_rtp_endpoint_connect_payloader_cb (KmsIRtpConnection * conn,
    gpointer d)
{
  ConnectPayloaderData *data = d;

  kms_base_rtp_endpoint_do_connect_payloader (data->self, data->payloader,
      data->connected_flag, data->type);
}

static void
kms_base_rtp_endpoint_connect_payloader_async (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, GstElement * payloader, gboolean * connected_flag,
    KmsElementPadType type)
{
  ConnectPayloaderData *data;
  gboolean connected = FALSE;
  gulong handler_id = 0;

  data = connect_payloader_data_new (self, payloader, connected_flag, type);

  handler_id = g_signal_connect_data (conn, "connected",
      G_CALLBACK (kms_base_rtp_endpoint_connect_payloader_cb), data,
      connect_payloader_data_destroy, 0);

  g_object_get (conn, "connected", &connected, NULL);

  if (connected) {
    if (handler_id) {
      g_signal_handler_disconnect (conn, handler_id);
    }

    kms_base_rtp_endpoint_do_connect_payloader (self, payloader, connected_flag,
        type);
  } else {
    GST_DEBUG_OBJECT (self, "Media not connected, waiting for signal");
  }
}

static void
kms_base_rtp_endpoint_connect_payloader (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, KmsElementPadType type, GstElement * payloader,
    gboolean * connected_flag, const gchar * rtpbin_pad_name)
{
  GstElement *rtpbin = self->priv->rtpbin;
  GstElement *rtprtxqueue = gst_element_factory_make ("rtprtxqueue", NULL);

  g_object_set (rtprtxqueue, "max-size-packets", 128, NULL);

  g_object_ref (payloader);
  gst_bin_add_many (GST_BIN (self), payloader, rtprtxqueue, NULL);
  gst_element_sync_state_with_parent (payloader);
  gst_element_sync_state_with_parent (rtprtxqueue);

  gst_element_link (payloader, rtprtxqueue);
  gst_element_link_pads (rtprtxqueue, "src", rtpbin, rtpbin_pad_name);

  kms_base_rtp_endpoint_connect_payloader_async (self, conn, payloader,
      connected_flag, type);
}

static gboolean
kms_base_rtp_endpoint_set_media_payloader (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *media_str = gst_sdp_media_get_media (media);
  GstElement *payloader;
  GstCaps *caps = NULL;
  guint j, f_len;
  const gchar *rtpbin_pad_name;
  KmsElementPadType type;
  gboolean *connected_flag;

  f_len = gst_sdp_media_formats_len (media);
  for (j = 0; j < f_len && caps == NULL; j++) {
    const gchar *pt = gst_sdp_media_get_format (media, j);
    const gchar *rtpmap = sdp_utils_sdp_media_get_rtpmap (media, pt);

    caps = kms_base_rtp_endpoint_get_caps_from_rtpmap (media_str, pt, rtpmap);
  }

  if (caps == NULL) {
    GST_WARNING_OBJECT (self, "Caps not found for media '%s'", media_str);
    return TRUE;
  }

  GST_DEBUG_OBJECT (self, "Found caps: %" GST_PTR_FORMAT, caps);

  payloader = gst_base_rtp_get_payloader_for_caps (caps);
  gst_caps_unref (caps);

  if (payloader == NULL) {
    GST_WARNING_OBJECT (self, "Payloader not found for media '%s'", media_str);
    return TRUE;
  }

  GST_DEBUG_OBJECT (self, "Found payloader %" GST_PTR_FORMAT, payloader);

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    self->priv->audio_payloader = payloader;
    connected_flag = &self->priv->audio_payloader_connected;
    type = KMS_ELEMENT_PAD_TYPE_AUDIO;
    rtpbin_pad_name = AUDIO_RTPBIN_SEND_RTP_SINK;
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    self->priv->video_payloader = payloader;
    connected_flag = &self->priv->video_payloader_connected;
    type = KMS_ELEMENT_PAD_TYPE_VIDEO;
    rtpbin_pad_name = VIDEO_RTPBIN_SEND_RTP_SINK;
  } else {
    rtpbin_pad_name = NULL;
    connected_flag = NULL;
    g_object_unref (payloader);
  }

  if (rtpbin_pad_name != NULL) {
    KmsIRtpConnection *conn;

    conn = kms_base_rtp_endpoint_get_connection (self, mconf);
    if (conn == NULL) {
      return TRUE;
    }

    kms_base_rtp_endpoint_connect_payloader (self, conn, type, payloader,
        connected_flag, rtpbin_pad_name);
  }

  return TRUE;
}

/* Payloading configuration end */

static void
kms_base_rtp_endpoint_connect_input_elements (KmsBaseSdpEndpoint *
    base_endpoint, SdpMessageContext * negotiated_ctx)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_endpoint);
  const GSList *item = kms_sdp_message_context_get_medias (negotiated_ctx);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;

    if (kms_sdp_media_config_is_inactive (mconf)) {
      gint mid = kms_sdp_media_config_get_id (mconf);

      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    /* TODO: connect only if is configured for sending */
    if (!kms_base_rtp_endpoint_set_media_payloader (self, mconf)) {
      GST_WARNING_OBJECT (self, "Cannot configure payloader.");
    }
  }
}

/* Connect input elements end */

static void
complete_caps_with_fb (GstCaps * caps, const GstSDPMedia * media,
    const gchar * payload)
{
  gboolean fir, pli;
  guint a;

  fir = pli = FALSE;

  for (a = 0;; a++) {
    const gchar *attr;

    attr = gst_sdp_media_get_attribute_val_n (media, RTCP_FB, a);
    if (attr == NULL) {
      break;
    }

    if (rtcp_fb_attr_check_type (attr, payload, RTCP_FB_FIR)) {
      fir = TRUE;
      continue;
    }

    if (rtcp_fb_attr_check_type (attr, payload, RTCP_FB_PLI)) {
      pli = TRUE;
      continue;
    }
  }

  gst_caps_set_simple (caps, "rtcp-fb-ccm-fir",
      G_TYPE_BOOLEAN, fir, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, pli, NULL);
}

static GstCaps *
kms_base_rtp_endpoint_get_caps_for_pt (KmsBaseRtpEndpoint * self, guint pt)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *negotiated_ctx;
  const GSList *item;

  negotiated_ctx = kms_base_sdp_endpoint_get_negotiated_sdp_ctx (base_endpoint);
  if (negotiated_ctx == NULL) {
    GST_WARNING_OBJECT (self, "Negotiation ctx not set");
    return NULL;
  }

  item = kms_sdp_message_context_get_medias (negotiated_ctx);
  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
    const gchar *media_str = gst_sdp_media_get_media (media);
    const gchar *rtpmap;
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
          kms_base_rtp_endpoint_get_caps_from_rtpmap (media_str, payload,
          rtpmap);

      if (caps != NULL) {
        complete_caps_with_fb (caps, media, payload);
        return caps;
      }
    }
  }

  return NULL;
}

static GstCaps *
kms_base_rtp_endpoint_request_pt_map (GstElement * rtpbin, guint session,
    guint pt, KmsBaseRtpEndpoint * self)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (self, "Caps request for pt: %d", pt);

  /* TODO: we will need to use the session if medias share payload numbers */
  caps = kms_base_rtp_endpoint_get_caps_for_pt (self, pt);

  if (caps != NULL) {
    return caps;
  }

  caps =
      gst_caps_new_simple ("application/x-rtp", "payload", G_TYPE_INT, pt,
      NULL);

  GST_WARNING_OBJECT (self, "Caps not found pt: %d. Setting: %" GST_PTR_FORMAT,
      pt, caps);

  return caps;
}

static void
kms_base_rtp_endpoint_rtpbin_pad_added (GstElement * rtpbin, GstPad * pad,
    KmsBaseRtpEndpoint * self)
{
  GstElement *agnostic, *depayloader;
  gboolean added = TRUE;
  KmsMediaType media;
  GstCaps *caps;

  GST_PAD_STREAM_LOCK (pad);

  if (g_str_has_prefix (GST_OBJECT_NAME (pad), AUDIO_RTPBIN_RECV_RTP_SRC)) {
    agnostic = kms_element_get_audio_agnosticbin (KMS_ELEMENT (self));
    media = KMS_MEDIA_TYPE_AUDIO;
  } else if (g_str_has_prefix (GST_OBJECT_NAME (pad),
          VIDEO_RTPBIN_RECV_RTP_SRC)) {
    agnostic = kms_element_get_video_agnosticbin (KMS_ELEMENT (self));
    media = KMS_MEDIA_TYPE_VIDEO;

    if (self->priv->rl != NULL) {
      self->priv->rl->event_manager = kms_utils_remb_event_manager_create (pad);
    }
  } else {
    added = FALSE;
    goto end;
  }

  caps = gst_pad_query_caps (pad, NULL);
  GST_DEBUG_OBJECT (self,
      "New pad: %" GST_PTR_FORMAT " for linking to %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, pad, agnostic, caps);

  depayloader = gst_base_rtp_get_depayloader_for_caps (caps);
  gst_caps_unref (caps);

  if (depayloader != NULL) {
    GST_DEBUG_OBJECT (self, "Found depayloader %" GST_PTR_FORMAT, depayloader);

    gst_bin_add (GST_BIN (self), depayloader);
    gst_element_link_pads (depayloader, "src", agnostic, "sink");
    gst_element_link_pads (rtpbin, GST_OBJECT_NAME (pad), depayloader, "sink");
    gst_element_sync_state_with_parent (depayloader);
  } else {
    GstElement *fake = gst_element_factory_make ("fakesink", NULL);

    GST_WARNING_OBJECT (self, "Depayloder not found for pad %" GST_PTR_FORMAT,
        pad);

    gst_bin_add (GST_BIN (self), fake);
    gst_element_link_pads (rtpbin, GST_OBJECT_NAME (pad), fake, "sink");
    gst_element_sync_state_with_parent (fake);
  }

end:
  GST_PAD_STREAM_UNLOCK (pad);

  if (added) {
    g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_START], 0, media, TRUE);
  }
}

static GstPadProbeReturn
kms_base_rtp_endpoint_change_latency_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer gp)
{
  GstElement *jitterbuffer = GST_PAD_PARENT (pad);

  g_object_set (jitterbuffer, "latency", JB_READY_LATENCY, NULL);

  return GST_PAD_PROBE_REMOVE;
}

static void
kms_base_rtp_endpoint_rtpbin_new_jitterbuffer (GstElement * rtpbin,
    GstElement * jitterbuffer,
    guint session, guint ssrc, KmsBaseRtpEndpoint * self)
{
  KmsRTPSessionStats *rtp_stats;
  KmsSSRCStats *ssrc_stats;
  GstPad *src_pad;

  g_object_set (jitterbuffer, "mode", 4 /* synced */ ,
      "latency", JB_INITIAL_LATENCY, NULL);

  src_pad = gst_element_get_static_pad (jitterbuffer, "src");
  gst_pad_add_probe (src_pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      kms_base_rtp_endpoint_change_latency_probe, NULL, NULL);
  g_object_unref (src_pad);

  KMS_ELEMENT_LOCK (self);

  rtp_stats =
      g_hash_table_lookup (self->priv->stats, GUINT_TO_POINTER (session));

  if (rtp_stats != NULL) {
    ssrc_stats = ssrc_stats_new (ssrc, jitterbuffer);
    rtp_stats->ssrcs = g_slist_prepend (rtp_stats->ssrcs, ssrc_stats);
  } else {
    GST_ERROR_OBJECT (self, "Session %u exists for SSRC %u", session, ssrc);
  }

  if (session == VIDEO_RTP_SESSION) {
    gboolean rtcp_nack = kms_base_rtp_endpoint_is_video_rtcp_nack (self);

    g_object_set (jitterbuffer, "do-lost", TRUE,
        "do-retransmission", rtcp_nack,
        "rtx-next-seqnum", FALSE,
        "rtx-max-retries", 0, /*"rtp-max-dropout", -1, */ NULL);
  }

  KMS_ELEMENT_UNLOCK (self);
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
ssrc_stats_add_jitter_stats (GstStructure * ssrc_stats,
    GstElement * jitter_buffer)
{
  GstStructure *jitter_stats;
  guint percent, latency;

  g_object_get (jitter_buffer, "percent", &percent, "latency", &latency,
      "stats", &jitter_stats, NULL);

  if (jitter_stats == NULL)
    return;

  /* Append adition fields to the stats */
  gst_structure_set (jitter_stats, "latency", G_TYPE_UINT, latency, "percent",
      G_TYPE_UINT, percent, NULL);

  /* Append jitter buffer stats to the ssrc stats */
  gst_structure_set (ssrc_stats, "jitter-buffer", GST_TYPE_STRUCTURE,
      jitter_stats, NULL);

  gst_structure_free (jitter_stats);
}

static GstElement *
rtp_session_stats_get_jitter_buffer (KmsRTPSessionStats * rtp_stats, guint ssrc)
{
  GSList *e;

  for (e = rtp_stats->ssrcs; e != NULL; e = e->next) {
    KmsSSRCStats *ssrc_stats = e->data;

    if (ssrc_stats->ssrc == ssrc)
      return ssrc_stats->jitter_buffer;
  }

  return NULL;
}

static void
append_rtp_session_stats (gpointer * session, KmsRTPSessionStats * rtp_stats,
    GstStructure * stats)
{
  GstStructure *session_stats;
  gchar *str_session;
  GValueArray *arr;
  guint i;

  g_object_get (rtp_stats->rtp_session, "stats", &session_stats, NULL);

  if (session_stats == NULL)
    return;

  /* Get stats for each source */
  g_object_get (rtp_stats->rtp_session, "sources", &arr, NULL);

  for (i = 0; i < arr->n_values; i++) {
    GstElement *jitter_buffer;
    GstStructure *ssrc_stats;
    GObject *source;
    GValue *val;
    gchar *name;
    guint ssrc;
    gchar *id;

    val = g_value_array_get_nth (arr, i);
    source = g_value_get_object (val);

    id = g_object_get_data (source, KMS_KEY_ID);

    if (id == NULL) {
      assign_uuid (source);
      id = g_object_get_data (source, KMS_KEY_ID);
    }

    g_object_get (source, "stats", &ssrc_stats, "ssrc", &ssrc, NULL);
    gst_structure_set (ssrc_stats, "id", G_TYPE_STRING, id, NULL);

    jitter_buffer = rtp_session_stats_get_jitter_buffer (rtp_stats, ssrc);

    if (jitter_buffer != NULL) {
      ssrc_stats_add_jitter_stats (ssrc_stats, jitter_buffer);
    }

    name = g_strdup_printf ("ssrc-%u", ssrc);
    gst_structure_set (session_stats, name, GST_TYPE_STRUCTURE, ssrc_stats,
        NULL);

    gst_structure_free (ssrc_stats);
    g_free (name);
  }

  g_value_array_free (arr);

  str_session = g_strdup_printf ("session-%u", GPOINTER_TO_UINT (session));
  gst_structure_set (stats, str_session, GST_TYPE_STRUCTURE, session_stats,
      NULL);

  gst_structure_free (session_stats);
  g_free (str_session);
}

static GstStructure *
kms_base_rtp_endpoint_create_stats (KmsBaseRtpEndpoint * self)
{
  GstStructure *stats;

  stats = gst_structure_new_empty ("stats");

  g_hash_table_foreach (self->priv->stats, (GHFunc) append_rtp_session_stats,
      stats);

  return stats;
}

static void
kms_base_rtp_endpoint_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (property_id) {
    case PROP_RTCP_MUX:
      self->priv->rtcp_mux = g_value_get_boolean (value);
      break;
    case PROP_RTCP_NACK:
      self->priv->rtcp_nack = g_value_get_boolean (value);
      break;
    case PROP_RTCP_REMB:
      self->priv->rtcp_remb = g_value_get_boolean (value);
      break;
    case PROP_TARGET_BITRATE:
      self->priv->target_bitrate = g_value_get_int (value);
      break;
    case PROP_MIN_VIDEO_SEND_BW:{
      guint v = g_value_get_uint (value);

      if (v > self->priv->max_video_send_bw) {
        v = self->priv->max_video_send_bw;
        GST_WARNING_OBJECT (object,
            "Trying to set min > max. Setting %" G_GUINT32_FORMAT, v);
      }

      self->priv->min_video_send_bw = v;
      break;
    }
    case PROP_MAX_VIDEO_SEND_BW:{
      guint v = g_value_get_uint (value);

      if (v < self->priv->min_video_send_bw) {
        v = self->priv->min_video_send_bw;
        GST_WARNING_OBJECT (object,
            "Trying to set max < min. Setting %" G_GUINT32_FORMAT, v);
      }

      self->priv->max_video_send_bw = v;
      break;
    }
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
    case PROP_RTCP_MUX:
      g_value_set_boolean (value, self->priv->rtcp_mux);
      break;
    case PROP_RTCP_NACK:
      g_value_set_boolean (value, self->priv->rtcp_nack);
      break;
    case PROP_RTCP_REMB:
      g_value_set_boolean (value, self->priv->rtcp_remb);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_int (value, self->priv->target_bitrate);
      break;
    case PROP_MIN_VIDEO_SEND_BW:
      g_value_set_uint (value, self->priv->min_video_send_bw);
      break;
    case PROP_MAX_VIDEO_SEND_BW:
      g_value_set_uint (value, self->priv->max_video_send_bw);
      break;
    case PROP_STATE:
      g_value_set_enum (value, self->priv->state);
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

  GST_DEBUG_OBJECT (self, "dispose");

  g_clear_object (&self->priv->audio_payloader);
  g_clear_object (&self->priv->video_payloader);

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

  kms_remb_local_destroy (self->priv->rl);
  kms_remb_remote_destroy (self->priv->rm);

  g_hash_table_destroy (self->priv->conns);
  g_hash_table_destroy (self->priv->stats);

  G_OBJECT_CLASS (kms_base_rtp_endpoint_parent_class)->finalize (gobject);
}

typedef struct _KmsRembStats
{
  GstStructure *stats;
  guint session;
} KmsRembStats;

static const GstStructure *
get_structure_from_id (const GstStructure * structure, const gchar * fieldname)
{
  const GValue *value;

  if (!gst_structure_has_field (structure, fieldname)) {
    GST_WARNING ("No stats for %s", fieldname);
    return NULL;
  }

  value = gst_structure_get_value (structure, fieldname);

  if (!GST_VALUE_HOLDS_STRUCTURE (value)) {
    gchar *str_val;

    str_val = g_strdup_value_contents (value);
    GST_WARNING ("Unexpected field type (%s) = %s", fieldname, str_val);
    g_free (str_val);

    return NULL;
  }

  return gst_value_get_structure (value);
}

static void
merge_remb_stats (gpointer key, guint * value, KmsRembStats * rs)
{
  guint ssrc = GPOINTER_TO_UINT (key);
  gchar *session_id, *ssrc_id;
  const GstStructure *session_stats, *ssrc_stats;

  session_id = g_strdup_printf ("session-%u", rs->session);
  session_stats = get_structure_from_id (rs->stats, session_id);
  g_free (session_id);

  if (session_stats == NULL) {
    return;
  }

  ssrc_id = g_strdup_printf ("ssrc-%u", ssrc);
  ssrc_stats = get_structure_from_id (session_stats, ssrc_id);
  g_free (ssrc_id);

  if (ssrc_stats == NULL) {
    return;
  }

  gst_structure_set ((GstStructure *) ssrc_stats, "remb", G_TYPE_UINT, *value,
      NULL);
}

static void
kms_base_rtp_endpoint_append_remb_stats (KmsBaseRtpEndpoint * self,
    GstStructure * stats)
{
  KmsRembStats rs;

  if (self->priv->rl != NULL) {
    KMS_REMB_BASE_LOCK (self->priv->rl);
    rs.stats = stats;
    rs.session = KMS_REMB_BASE (self->priv->rl)->session;
    g_hash_table_foreach (KMS_REMB_BASE (self->priv->rl)->remb_stats,
        (GHFunc) merge_remb_stats, &rs);
    KMS_REMB_BASE_UNLOCK (self->priv->rl);
  }

  if (self->priv->rm != NULL) {
    KMS_REMB_BASE_LOCK (self->priv->rm);
    rs.stats = stats;
    rs.session = KMS_REMB_BASE (self->priv->rm)->session;
    g_hash_table_foreach (KMS_REMB_BASE (self->priv->rm)->remb_stats,
        (GHFunc) merge_remb_stats, &rs);
    KMS_REMB_BASE_UNLOCK (self->priv->rm);
  }
}

GstStructure *
kms_base_rtp_endpoint_stats_action (KmsIStats * obj)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (obj);
  GstStructure *stats;

  stats = kms_base_rtp_endpoint_create_stats (self);

  kms_base_rtp_endpoint_append_remb_stats (self, stats);

  return stats;
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

  klass->request_local_key_frame =
      kms_base_rtp_endpoint_request_local_key_frame;

  /* Connection management */
  klass->create_connection = kms_base_rtp_endpoint_create_connection_default;
  klass->create_rtcp_mux_connection =
      kms_base_rtp_endpoint_create_rtcp_mux_connection_default;
  klass->create_bundle_connection =
      kms_base_rtp_endpoint_create_bundle_connection_default;

  base_endpoint_class = KMS_BASE_SDP_ENDPOINT_CLASS (klass);
  base_endpoint_class->start_transport_send =
      kms_base_rtp_endpoint_start_transport_send;
  base_endpoint_class->connect_input_elements =
      kms_base_rtp_endpoint_connect_input_elements;

  /* Media handler management */
  base_endpoint_class->create_media_handler = kms_base_rtp_create_media_handler;

  base_endpoint_class->configure_media = kms_base_rtp_endpoint_configure_media;

  g_object_class_install_property (object_class, PROP_STATE,
      g_param_spec_enum ("state", "Media state", "Media state",
          KMS_TYPE_MEDIA_STATE, KMS_MEDIA_STATE_DISCONNECTED,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_RTCP_MUX,
      g_param_spec_boolean ("rtcp-mux", "RTCP mux",
          "RTCP mux", DEFAULT_RTCP_MUX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_RTCP_NACK,
      g_param_spec_boolean ("rtcp-nack", "RTCP NACK",
          "RTCP NACK", DEFAULT_RTCP_NACK,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_RTCP_REMB,
      g_param_spec_boolean ("rtcp-remb", "RTCP REMB",
          "RTCP REMB", DEFAULT_RTCP_REMB,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_TARGET_BITRATE,
      g_param_spec_int ("target-bitrate", "Target bitrate",
          "Target bitrate (bps)", 0, G_MAXINT,
          DEFAULT_TARGET_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MIN_VIDEO_SEND_BW,
      g_param_spec_uint ("min-video-send-bandwidth",
          "Minimum video bandwidth for sending",
          "Minimum video bandwidth for sending. Unit: kbps(kilobits per second). 0: unlimited",
          0, G_MAXUINT32, MIN_VIDEO_SEND_BW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MAX_VIDEO_SEND_BW,
      g_param_spec_uint ("max-video-send-bandwidth",
          "Maximum video bandwidth for sending",
          "Maximum video bandwidth for sending. Unit: kbps(kilobits per second). 0: unlimited",
          0, G_MAXUINT32, MAX_VIDEO_SEND_BW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* set signals */
  obj_signals[MEDIA_STATE_CHANGED] =
      g_signal_new ("media-state-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, media_state_changed), NULL,
      NULL, g_cclosure_marshal_VOID__ENUM, G_TYPE_NONE, 1,
      KMS_TYPE_MEDIA_STATE);

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

  obj_signals[SIGNAL_REQUEST_LOCAL_KEY_FRAME] =
      g_signal_new ("request-local-key-frame",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, request_local_key_frame), NULL,
      NULL, __kms_core_marshal_BOOLEAN__VOID, G_TYPE_BOOLEAN, 0);

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
      if (self->priv->audio_ssrc != 0) {
        break;
      }

      self->priv->audio_ssrc = ssrc;
      break;
    case VIDEO_RTP_SESSION:
      if (self->priv->video_ssrc != 0) {
        break;
      }

      self->priv->video_ssrc = ssrc;
      break;
    default:
      GST_WARNING_OBJECT (self, "No media supported for session %u", session);
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_rtp_endpoint_set_media_state (KmsBaseRtpEndpoint * self, guint session,
    KmsMediaState state)
{
  gboolean actived = FALSE, emit = FALSE;
  KmsMediaState new_state;

  KMS_ELEMENT_LOCK (self);

  actived = state == KMS_MEDIA_STATE_CONNECTED;

  switch (session) {
    case AUDIO_RTP_SESSION:
      self->priv->audio_actived = actived;
      break;
    case VIDEO_RTP_SESSION:
      self->priv->video_actived = actived;
      break;
    default:
      GST_WARNING_OBJECT (self, "No media supported for session %u", session);
  }

  if (self->priv->audio_actived || self->priv->video_actived) {
    /* There is still a media connection alive */
    new_state = KMS_MEDIA_STATE_CONNECTED;
  } else {
    new_state = KMS_MEDIA_STATE_DISCONNECTED;
  }

  if (self->priv->state != new_state) {
    self->priv->state = new_state;
    emit = TRUE;
  }

  KMS_ELEMENT_UNLOCK (self);

  if (emit) {
    g_signal_emit (G_OBJECT (self), obj_signals[MEDIA_STATE_CHANGED], 0,
        new_state);
  }
}

static void
kms_base_rtp_endpoint_rtpbin_on_bye_ssrc (GstElement * rtpbin, guint session,
    guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  kms_base_rtp_endpoint_set_media_state (self, session,
      KMS_MEDIA_STATE_DISCONNECTED);

  kms_base_rtp_endpoint_stop_signal (self, session, ssrc);
}

static void
kms_base_rtp_endpoint_rtpbin_on_bye_timeout (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  kms_base_rtp_endpoint_set_media_state (self, session,
      KMS_MEDIA_STATE_DISCONNECTED);

  kms_base_rtp_endpoint_stop_signal (self, session, ssrc);
}

static void
kms_base_rtp_endpoint_rtpbin_on_ssrc_sdes (GstElement * rtpbin, guint session,
    guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);
  KmsMediaType media;

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
}

static void
kms_base_rtp_endpoint_rtpbin_on_sender_timeout (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  kms_base_rtp_endpoint_set_media_state (self, session,
      KMS_MEDIA_STATE_DISCONNECTED);

  kms_base_rtp_endpoint_stop_signal (self, session, ssrc);
}

static void
kms_base_rtp_endpoint_rtpbin_on_timeout (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  kms_base_rtp_endpoint_set_media_state (self, session,
      KMS_MEDIA_STATE_DISCONNECTED);
}

static void
kms_base_rtp_endpoint_rtpbin_on_ssrc_active (GstElement * rtpbin,
    guint session, guint ssrc, gpointer user_data)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (user_data);

  kms_base_rtp_endpoint_set_media_state (self, session,
      KMS_MEDIA_STATE_CONNECTED);
}

static void
kms_base_rtp_endpoint_init (KmsBaseRtpEndpoint * self)
{
  self->priv = KMS_BASE_RTP_ENDPOINT_GET_PRIVATE (self);
  self->priv->rtcp_mux = DEFAULT_RTCP_MUX;
  self->priv->rtcp_nack = DEFAULT_RTCP_NACK;
  self->priv->rtcp_remb = DEFAULT_RTCP_REMB;

  self->priv->min_video_send_bw = MIN_VIDEO_SEND_BW_DEFAULT;
  self->priv->max_video_send_bw = MAX_VIDEO_SEND_BW_DEFAULT;

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

  g_signal_connect (self->priv->rtpbin, "on-timeout",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_timeout), self);

  g_signal_connect (self->priv->rtpbin, "on-ssrc-active",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_ssrc_active), self);

  g_object_set (self, "accept-eos", FALSE, "do-synchronization", TRUE, NULL);

  gst_bin_add (GST_BIN (self), self->priv->rtpbin);

  self->priv->audio_payloader = NULL;
  self->priv->video_payloader = NULL;

  self->priv->audio_payloader_connected = FALSE;
  self->priv->video_payloader_connected = FALSE;

  self->priv->conns =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  self->priv->stats = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) rtp_session_stats_destroy);
}

static void
kms_istats_interface_init (KmsIStatsInterface * iface)
{
  iface->stats = kms_base_rtp_endpoint_stats_action;
}
