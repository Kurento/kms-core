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
#include "kmsbasertpsession.h"
#include "constants.h"

#include <uuid/uuid.h>
#include <stdlib.h>

#include "kms-core-enumtypes.h"
#include "kms-core-marshal.h"
#include "sdp_utils.h"
#include "sdpagent/kmssdprtpavpfmediahandler.h"
#include "kmsremb.h"
#include "kmsrefstruct.h"

#include <gst/rtp/gstrtpdefs.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/video/video-event.h>
#include "kmsbufferlacentymeta.h"
#include "kmsstats.h"

#define PLUGIN_NAME "base_rtp_endpoint"

GST_DEBUG_CATEGORY_STATIC (kms_base_rtp_endpoint_debug);
#define GST_CAT_DEFAULT kms_base_rtp_endpoint_debug

#define kms_base_rtp_endpoint_parent_class parent_class
#define UUID_STR_SIZE 37        /* 36-byte string (plus tailing '\0') */
#define KMS_KEY_ID "kms-key-id"

static void
kms_i_rtp_session_manager_interface_init (KmsIRtpSessionManagerInterface *
    iface);

G_DEFINE_TYPE_WITH_CODE (KmsBaseRtpEndpoint, kms_base_rtp_endpoint,
    KMS_TYPE_BASE_SDP_ENDPOINT,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_RTP_SESSION_MANAGER,
        kms_i_rtp_session_manager_interface_init)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME));

#define KMS_BASE_RTP_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                   \
    (obj),                                        \
    KMS_TYPE_BASE_RTP_ENDPOINT,                   \
    KmsBaseRtpEndpointPrivate                     \
  )                                               \
)

#define JB_INITIAL_LATENCY 0
#define JB_READY_AUDIO_LATENCY 200
#define JB_READY_VIDEO_LATENCY 1500

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
  GstSDPDirection direction;
  GSList *ssrcs;                /* list of all jitter buffers associated to a ssrc */
};

typedef struct _KmsBaseRTPStats KmsBaseRTPStats;
struct _KmsBaseRTPStats
{
  gboolean enabled;
  GHashTable *rtp_stats;
  GSList *probes;
  GMutex mutex;
  gdouble vi;
  gdouble ai;
};

struct _KmsBaseRtpEndpointPrivate
{
  GstElement *rtpbin;
  KmsMediaState media_state;
  gboolean audio_actived;
  gboolean video_actived;

  gboolean rtcp_mux;
  gboolean rtcp_nack;
  gboolean rtcp_remb;

  GstElement *audio_payloader;
  GstElement *video_payloader;

  gboolean audio_payloader_connected;
  gboolean video_payloader_connected;

  guint local_audio_ssrc;
  guint audio_ssrc;

  guint local_video_ssrc;
  guint video_ssrc;

  gint32 target_bitrate;
  guint min_video_recv_bw;
  guint min_video_send_bw;
  guint max_video_send_bw;

  /* REMB */
  GstStructure *remb_params;
  KmsRembLocal *rl;
  KmsRembRemote *rm;

  /* RTP statistics */
  KmsBaseRTPStats stats;
};

/* Signals and args */
enum
{
  MEDIA_START,
  MEDIA_STOP,
  MEDIA_STATE_CHANGED,
  GET_CONNECTION_STATE,
  CONNECTION_STATE_CHANGED,
  SIGNAL_REQUEST_LOCAL_KEY_FRAME,
  LAST_SIGNAL
};

static guint obj_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_RTCP_MUX    FALSE
#define DEFAULT_RTCP_NACK    FALSE
#define DEFAULT_RTCP_REMB    FALSE
#define DEFAULT_TARGET_BITRATE    0
#define MIN_VIDEO_RECV_BW_DEFAULT 0
#define MIN_VIDEO_SEND_BW_DEFAULT 100
#define MAX_VIDEO_SEND_BW_DEFAULT 500

enum
{
  PROP_0,
  PROP_RTCP_MUX,
  PROP_RTCP_NACK,
  PROP_RTCP_REMB,
  PROP_TARGET_BITRATE,
  PROP_MIN_VIDEO_RECV_BW,
  PROP_MIN_VIDEO_SEND_BW,
  PROP_MAX_VIDEO_SEND_BW,
  PROP_MEDIA_STATE,
  PROP_REMB_PARAMS,
  PROP_LAST
};

/* RTP hdrext begin */

typedef struct _HdrExtData
{
  GstPad *pad;
  /* Useful to make buffers writable when needed. */
  gboolean add_hdr;
  gboolean set_time;
  gint abs_send_time_id;
} HdrExtData;

static HdrExtData *
hdr_ext_data_new (GstPad * pad, gboolean add_hdr, gboolean set_time,
    gint abs_send_time_id)
{
  HdrExtData *data;

  data = g_slice_new0 (HdrExtData);
  data->pad = pad;
  data->add_hdr = add_hdr;
  data->set_time = set_time;
  data->abs_send_time_id = abs_send_time_id;

  return data;
}

static void
hdr_ext_data_destroy (HdrExtData * data)
{
  g_slice_free (HdrExtData, data);
}

static void
hdr_ext_data_destroy_pointer (gpointer data)
{
  hdr_ext_data_destroy ((HdrExtData *) data);
}

static void
kms_base_rtp_endpoint_rtp_hdr_ext_set_time (guint8 * data)
{
  GstClockTime current_time, ms;
  guint value;

  current_time = kms_utils_get_time_nsecs ();
  ms = current_time / 1000000;
  value = (((ms << 18) / 1000) & 0x00ffffff);

  data[0] = (guint8) (value >> 16);
  data[1] = (guint8) (value >> 8);
  data[2] = (guint8) (value);
}

static void
kms_base_rtp_endpoint_add_rtp_hdr_ext (HdrExtData * data, GstBuffer * buffer)
{
  GstRTPBuffer rtp = { NULL, };
  guint8 id = data->abs_send_time_id;
  GstMapFlags map_flags;
  guint8 *time;
  guint size;

  if (data->add_hdr) {
    map_flags = GST_MAP_WRITE;
  } else {
    map_flags = GST_MAP_READ;
  }

  if (!gst_rtp_buffer_map (buffer, map_flags, &rtp)) {
    GST_WARNING_OBJECT (data->pad, "Can not map RTP buffer");
    return;
  }

  if (!gst_rtp_buffer_get_extension_onebyte_header (&rtp,
          id, 0, (gpointer) & time, &size)) {
    GST_TRACE_OBJECT (data->pad,
        "RTP hdrext abs-send-time with id '%d' not found", id);

    if (data->add_hdr) {
      GST_TRACE_OBJECT (data->pad, " Adding new one.");
    } else {
      GST_WARNING_OBJECT (data->pad, "Cannot add new one: not writable");
      goto end;
    }

    time = g_malloc0 (RTP_HDR_EXT_ABS_SEND_TIME_SIZE);
    if (data->set_time) {
      kms_base_rtp_endpoint_rtp_hdr_ext_set_time (time);
    }

    if (!gst_rtp_buffer_add_extension_onebyte_header (&rtp,
            id, time, RTP_HDR_EXT_ABS_SEND_TIME_SIZE)) {
      GST_WARNING_OBJECT (data->pad, "RTP hdrext abs-send-time not added");
    }

    g_free (time);
  } else if (data->set_time) {
    if (size != RTP_HDR_EXT_ABS_SEND_TIME_SIZE) {
      GST_WARNING_OBJECT (data->pad,
          "RTP hdrext abs-send-time size with id '%d' not matching", id);
    } else {
      GST_TRACE_OBJECT (data->pad,
          "RTP hdrext abs-send-time with id '%d' found. Update time.", id);
      kms_base_rtp_endpoint_rtp_hdr_ext_set_time (time);
    }
  }

end:
  gst_rtp_buffer_unmap (&rtp);
}

static gboolean
kms_base_rtp_endpoint_add_rtp_hdr_ext_bufflist (GstBuffer ** buf, guint idx,
    HdrExtData * data)
{
  if (data->add_hdr) {
    *buf = gst_buffer_make_writable (*buf);
  }
  kms_base_rtp_endpoint_add_rtp_hdr_ext (data, *buf);

  return TRUE;
}

static GstPadProbeReturn
kms_base_rtp_endpoint_add_rtp_hdr_ext_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer gp)
{
  HdrExtData *data = (HdrExtData *) gp;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

    if (data->add_hdr) {
      buffer = gst_buffer_make_writable (buffer);
    }
    kms_base_rtp_endpoint_add_rtp_hdr_ext (data, buffer);
    GST_PAD_PROBE_INFO_DATA (info) = buffer;
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *bufflist = GST_PAD_PROBE_INFO_BUFFER_LIST (info);

    if (data->add_hdr) {
      bufflist = gst_buffer_list_make_writable (bufflist);
    }
    gst_buffer_list_foreach (bufflist,
        (GstBufferListFunc) kms_base_rtp_endpoint_add_rtp_hdr_ext_bufflist,
        data);

    GST_PAD_PROBE_INFO_DATA (info) = bufflist;
  }

  return GST_PAD_PROBE_OK;
}

static void
kms_base_rtp_endpoint_config_rtp_hdr_ext (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf)
{
  HdrExtData *data;
  gint abs_send_time_id;
  GstPad *pad;

  abs_send_time_id = kms_sdp_media_config_get_abs_send_time_id (mconf);
  if (abs_send_time_id == -1) {
    GST_DEBUG_OBJECT (self, "abs-send-time-id not configured.");
    return;
  }

  pad = gst_element_get_static_pad (self->priv->video_payloader, "src");
  if (pad == NULL) {
    GST_WARNING_OBJECT (self, "No RTP pad to configure hdrext probe.");
    return;
  }

  data = hdr_ext_data_new (pad, TRUE, FALSE, abs_send_time_id);

  GST_DEBUG_OBJECT (self,
      "Add probe for adding abs-send-time (id: %d, %" GST_PTR_FORMAT
      ").", abs_send_time_id, pad);
  gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      kms_base_rtp_endpoint_add_rtp_hdr_ext_probe, data,
      hdr_ext_data_destroy_pointer);
  g_object_unref (pad);
}

/* RTP hdrext end */

/* Media handler management begin */
static void
kms_base_rtp_create_media_handler (KmsBaseSdpEndpoint * base_sdp,
    const gchar * media, KmsSdpMediaHandler ** handler)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp);
  KmsSdpRtpAvpMediaHandler *h_avp;
  GError *err = NULL;

  if (*handler == NULL) {
    /* Media not supported */
    return;
  }

  if (!KMS_IS_SDP_RTP_AVP_MEDIA_HANDLER (*handler)) {
    /* No further configuration required */
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
  KmsRefStruct ref;
  KmsBaseRtpEndpoint *self;
  GstElement *payloader;
  gboolean *connected_flag;
  KmsElementPadType type;
} ConnectPayloaderData;

static void
connect_payloader_data_destroy (gpointer data)
{
  g_slice_free (ConnectPayloaderData, data);
}

static ConnectPayloaderData *
connect_payloader_data_new (KmsBaseRtpEndpoint * self, GstElement * payloader,
    gboolean * connected_flag, KmsElementPadType type)
{
  ConnectPayloaderData *data;

  data = g_slice_new0 (ConnectPayloaderData);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (data),
      (GDestroyNotify) connect_payloader_data_destroy);

  data->self = self;
  data->payloader = payloader;
  data->connected_flag = connected_flag;
  data->type = type;

  return data;
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
rtp_session_stats_new (GObject * rtp_session, GstSDPDirection direction)
{
  KmsRTPSessionStats *stats;

  stats = g_slice_new0 (KmsRTPSessionStats);
  stats->rtp_session = g_object_ref (rtp_session);
  stats->direction = direction;

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
kms_base_rtp_endpoint_is_video_rtcp_nack (KmsBaseRtpEndpoint * self)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *neg_sdp_ctx =
      kms_base_sdp_endpoint_get_first_negotiated_sdp_ctx (base_endpoint);
  const GSList *item;

  if (neg_sdp_ctx == NULL) {
    GST_WARNING_OBJECT (self, "Negotiated session not set");
    return FALSE;
  }

  item = kms_sdp_message_context_get_medias (neg_sdp_ctx);
  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
    const gchar *media_str = gst_sdp_media_get_media (media);

    if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
      return sdp_utils_media_has_rtcp_nack (media);
    }
  }

  return FALSE;
}

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
    guint session_id, const gchar * rtpbin_pad_name, guint rtp_profile,
    GstSDPDirection direction)
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

  g_mutex_lock (&self->priv->stats.mutex);

  rtp_stats =
      g_hash_table_lookup (self->priv->stats.rtp_stats,
      GUINT_TO_POINTER (session_id));

  if (rtp_stats == NULL) {
    rtp_stats = rtp_session_stats_new (rtpsession, direction);
    g_hash_table_insert (self->priv->stats.rtp_stats,
        GUINT_TO_POINTER (session_id), rtp_stats);
  } else {
    GST_WARNING_OBJECT (self, "Session %u already created", session_id);
  }

  g_mutex_unlock (&self->priv->stats.mutex);

  g_object_set (rtpsession, "rtcp-min-interval",
      RTCP_MIN_INTERVAL * GST_MSECOND, "rtp-profile", rtp_profile, NULL);

  return rtpsession;
}

static guint
kms_base_rtp_endpoint_media_proto_to_rtp_profile (KmsBaseRtpEndpoint * self,
    const gchar * proto)
{
  if (g_strcmp0 (proto, "RTP/AVP") == 0) {
    return GST_RTP_PROFILE_AVP;
  } else if (g_strcmp0 (proto, "RTP/AVPF") == 0) {
    return GST_RTP_PROFILE_AVPF;
  } else if (g_strcmp0 (proto, "RTP/SAVP") == 0) {
    return GST_RTP_PROFILE_SAVP;
  } else if (g_strcmp0 (proto, "RTP/SAVPF") == 0) {
    return GST_RTP_PROFILE_SAVPF;
  } else {
    GST_WARNING_OBJECT (self, "Unknown protocol '%s'", proto);
    return GST_RTP_PROFILE_UNKNOWN;
  }
}

static gboolean
kms_base_rtp_endpoint_configure_rtp_media (KmsBaseRtpEndpoint * self,
    KmsBaseRtpSession * base_rtp_sess, SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *proto_str = gst_sdp_media_get_proto (media);
  const gchar *media_str = gst_sdp_media_get_media (media);
  const gchar *rtpbin_pad_name = NULL;
  GstSDPDirection dir;
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

  dir =
      sdp_utils_media_config_get_direction (kms_sdp_media_config_get_sdp_media
      (mconf));

  rtpsession =
      kms_base_rtp_endpoint_create_rtp_session (self, session_id,
      rtpbin_pad_name,
      kms_base_rtp_endpoint_media_proto_to_rtp_profile (self, proto_str), dir);
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
    base_rtp_sess->local_audio_ssrc = ssrc;
  } else if (session_id == VIDEO_RTP_SESSION) {
    self->priv->local_video_ssrc = ssrc;
    base_rtp_sess->local_video_ssrc = ssrc;
  }

  return TRUE;
}

static gboolean
kms_base_rtp_endpoint_configure_media (KmsBaseSdpEndpoint *
    base_sdp_endpoint, KmsSdpSession * sess, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  KmsBaseRtpSession *base_rtp_sess = KMS_BASE_RTP_SESSION (sess);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_session_create_connection (base_rtp_sess, mconf);
  if (conn == NULL) {
    return FALSE;
  }

  return kms_base_rtp_endpoint_configure_rtp_media (self, base_rtp_sess, mconf);
}

/* Configure media SDP end */

/* Start Transport Send begin */

static void
kms_base_rtp_endpoint_create_remb_managers (KmsBaseRtpSession * sess,
    KmsBaseRtpEndpoint * self)
{
  GstElement *rtpbin = self->priv->rtpbin;
  GObject *rtpsession;
  GstPad *pad;
  int max_recv_bw;

  if (self->priv->rl != NULL) {
    /* TODO: support more than one media with REMB */
    GST_INFO_OBJECT (self, "Only support for one media with REMB");
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
      sess->remote_video_ssrc, self->priv->min_video_recv_bw, max_recv_bw);

  pad = gst_element_get_static_pad (rtpbin, VIDEO_RTPBIN_SEND_RTP_SINK);
  self->priv->rm =
      kms_remb_remote_create (rtpsession, VIDEO_RTP_SESSION,
      self->priv->local_video_ssrc, self->priv->min_video_send_bw,
      self->priv->max_video_send_bw, pad);
  g_object_unref (pad);
  g_object_unref (rtpsession);

  if (self->priv->remb_params != NULL) {
    kms_remb_local_set_params (self->priv->rl, self->priv->remb_params);
    kms_remb_remote_set_params (self->priv->rm, self->priv->remb_params);
  }

  GST_DEBUG_OBJECT (self, "REMB managers added");
}

static GstPad *
kms_base_rtp_endpoint_request_rtp_sink (KmsIRtpSessionManager * manager,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (manager);
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *media_str = gst_sdp_media_get_media (media);
  GstPad *pad;

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        AUDIO_RTPBIN_RECV_RTP_SINK);
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        VIDEO_RTPBIN_RECV_RTP_SINK);
  } else {
    GST_ERROR_OBJECT (self, "'%s' not valid", media_str);
    return NULL;
  }

  return pad;
}

static GstPad *
kms_base_rtp_endpoint_request_rtp_src (KmsIRtpSessionManager * manager,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (manager);
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *media_str = gst_sdp_media_get_media (media);
  GstPad *pad;

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_static_pad (self->priv->rtpbin,
        AUDIO_RTPBIN_SEND_RTP_SRC);
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    gint abs_send_time_id;

    pad =
        gst_element_get_static_pad (self->priv->rtpbin,
        VIDEO_RTPBIN_SEND_RTP_SRC);

    kms_utils_drop_until_keyframe (pad, TRUE);

    /* TODO: check if needed for audio */
    abs_send_time_id = kms_sdp_media_config_get_abs_send_time_id (mconf);
    if (abs_send_time_id != -1) {
      HdrExtData *data = hdr_ext_data_new (pad, FALSE, TRUE, abs_send_time_id);

      GST_DEBUG_OBJECT (self,
          "Add probe for updating abs-send-time (id: %d, %" GST_PTR_FORMAT ").",
          abs_send_time_id, pad);
      gst_pad_add_probe (pad,
          GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
          kms_base_rtp_endpoint_add_rtp_hdr_ext_probe,
          data, hdr_ext_data_destroy_pointer);
    }
  } else {
    GST_ERROR_OBJECT (self, "'%s' not valid", media_str);
    return NULL;
  }

  return pad;
}

static GstPad *
kms_base_rtp_endpoint_request_rtcp_sink (KmsIRtpSessionManager * manager,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (manager);
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *media_str = gst_sdp_media_get_media (media);
  GstPad *pad;

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        AUDIO_RTPBIN_RECV_RTCP_SINK);
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        VIDEO_RTPBIN_RECV_RTCP_SINK);
  } else {
    GST_ERROR_OBJECT (self, "'%s' not valid", media_str);
    return NULL;
  }

  return pad;
}

static GstPad *
kms_base_rtp_endpoint_request_rtcp_src (KmsIRtpSessionManager * manager,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (manager);
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *media_str = gst_sdp_media_get_media (media);
  GstPad *pad;

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        AUDIO_RTPBIN_SEND_RTCP_SRC);
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    pad =
        gst_element_get_request_pad (self->priv->rtpbin,
        VIDEO_RTPBIN_SEND_RTCP_SRC);
  } else {
    GST_ERROR_OBJECT (self, "'%s' not valid", media_str);
    return NULL;
  }

  return pad;
}

static KmsConnectionState
kms_base_rtp_endpoint_get_connection_state (KmsBaseRtpEndpoint * self,
    const gchar * sess_id)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  GHashTable *sessions;
  KmsBaseRtpSession *sess;
  KmsConnectionState ret = KMS_CONNECTION_STATE_DISCONNECTED;

  KMS_ELEMENT_LOCK (self);

  sessions = kms_base_sdp_endpoint_get_sessions (base_endpoint);
  sess = g_hash_table_lookup (sessions, sess_id);
  if (sess == NULL) {
    GST_WARNING_OBJECT (self, "There is not session '%s'", sess_id);
    goto end;
  }

  g_object_get (sess, "connection-state", &ret, NULL);

end:
  KMS_ELEMENT_UNLOCK (self);

  return ret;
}

static void
kms_base_rtp_endpoint_start_transport_send (KmsBaseSdpEndpoint *
    base_sdp_endpoint, KmsSdpSession * sess, gboolean offerer)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  KmsBaseRtpSession *base_rtp_sess = KMS_BASE_RTP_SESSION (sess);
  GSList *item = kms_sdp_message_context_get_medias (sess->neg_sdp_ctx);

  kms_base_rtp_session_start_transport_send (base_rtp_sess, offerer);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *neg_mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (neg_mconf);

    if (sdp_utils_media_has_remb (media)) {
      kms_base_rtp_endpoint_create_remb_managers (base_rtp_sess, self);
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
    GST_WARNING ("rtpmap is NULL for media '%s'", media);
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

  /* DTLS is already connected, so we do not need to be attached to this */
  /* signal any more. We can free the tmp data without waiting for the   */
  /* object to be realeased.                                             */
  g_signal_handlers_disconnect_by_data (conn, data);
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
      G_CALLBACK (kms_base_rtp_endpoint_connect_payloader_cb),
      kms_ref_struct_ref (KMS_REF_STRUCT_CAST (data)),
      (GClosureNotify) kms_ref_struct_unref, 0);

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

  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (data));
}

static void
kms_base_rtp_endpoint_connect_payloader (KmsBaseRtpEndpoint * self,
    KmsIRtpConnection * conn, KmsElementPadType type, GstElement * payloader,
    gboolean * connected_flag, const gchar * rtpbin_pad_name)
{
  GstElement *rtpbin = self->priv->rtpbin;
  GstElement *src_element;

  src_element = payloader;
  g_object_ref (payloader);
  gst_bin_add (GST_BIN (self), payloader);
  gst_element_sync_state_with_parent (payloader);

  /* TODO: add rtxqueue if the media has "nack" instead base on the media type */
  if (g_strcmp0 (VIDEO_RTPBIN_SEND_RTP_SINK, rtpbin_pad_name) == 0) {
    GstElement *rtprtxqueue = gst_element_factory_make ("rtprtxqueue", NULL);

    src_element = rtprtxqueue;
    g_object_set (rtprtxqueue, "max-size-packets", 512, NULL);
    gst_bin_add (GST_BIN (self), rtprtxqueue);
    gst_element_sync_state_with_parent (rtprtxqueue);
    gst_element_link (payloader, rtprtxqueue);
  }

  gst_element_link_pads (src_element, "src", rtpbin, rtpbin_pad_name);

  kms_base_rtp_endpoint_connect_payloader_async (self, conn, payloader,
      connected_flag, type);
}

static void
kms_base_rtp_endpoint_set_media_payloader (KmsBaseRtpEndpoint * self,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
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
    return;
  }

  GST_DEBUG_OBJECT (self, "Found caps: %" GST_PTR_FORMAT, caps);

  payloader = gst_base_rtp_get_payloader_for_caps (caps);
  gst_caps_unref (caps);

  if (payloader == NULL) {
    GST_WARNING_OBJECT (self, "Payloader not found for media '%s'", media_str);
    return;
  }

  GST_DEBUG_OBJECT (self, "Found payloader %" GST_PTR_FORMAT, payloader);

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    self->priv->audio_payloader = payloader;
    connected_flag = &self->priv->audio_payloader_connected;
    type = KMS_ELEMENT_PAD_TYPE_AUDIO;
    rtpbin_pad_name = AUDIO_RTPBIN_SEND_RTP_SINK;
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    self->priv->video_payloader = payloader;
    /* TODO: check if is needed for audio  */
    kms_base_rtp_endpoint_config_rtp_hdr_ext (self, mconf);
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

    conn = kms_base_rtp_session_get_connection (sess, mconf);
    if (conn == NULL) {
      return;
    }

    kms_base_rtp_endpoint_connect_payloader (self, conn, type, payloader,
        connected_flag, rtpbin_pad_name);
  }
}

/* Payloading configuration end */

static void
kms_base_rtp_endpoint_connect_input_elements (KmsBaseSdpEndpoint *
    base_endpoint, KmsSdpSession * sess)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_endpoint);
  KmsBaseRtpSession *base_rtp_sess = KMS_BASE_RTP_SESSION (sess);
  const GSList *item = kms_sdp_message_context_get_medias (sess->neg_sdp_ctx);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
    const gchar *media_str = gst_sdp_media_get_media (media);

    if (kms_sdp_media_config_is_inactive (mconf)) {
      gint mid = kms_sdp_media_config_get_id (mconf);

      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    if (g_strcmp0 (media_str, AUDIO_STREAM_NAME) == 0 ||
        g_strcmp0 (media_str, VIDEO_STREAM_NAME) == 0) {
      kms_base_rtp_endpoint_set_media_payloader (self, base_rtp_sess, mconf);
    }
  }
}

/* Connect input elements end */

static void
connection_state_changed (KmsSdpSession * sess, guint new_state,
    KmsBaseRtpEndpoint * self)
{
  g_signal_emit (self, obj_signals[CONNECTION_STATE_CHANGED], 0, sess->id_str,
      new_state);
}

static void
kms_base_rtp_endpoint_create_session_internal (KmsBaseSdpEndpoint * base_sdp,
    gint id, KmsSdpSession ** sess)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp);

  if (*sess == NULL) {
    /* Media not supported */
    return;
  }

  g_signal_connect (*sess, "connection-state-changed",
      (GCallback) connection_state_changed, self);
}

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

    if (sdp_utils_rtcp_fb_attr_check_type (attr, payload, RTCP_FB_FIR)) {
      fir = TRUE;
      continue;
    }

    if (sdp_utils_rtcp_fb_attr_check_type (attr, payload, RTCP_FB_PLI)) {
      pli = TRUE;
      continue;
    }
  }

  if (fir) {
    gst_caps_set_simple (caps, "rtcp-fb-ccm-fir", G_TYPE_BOOLEAN, fir, NULL);
  }
  if (pli) {
    gst_caps_set_simple (caps, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, pli, NULL);
  }
}

static GstCaps *
kms_base_rtp_endpoint_get_caps_for_pt (KmsBaseRtpEndpoint * self, guint pt)
{
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *neg_sdp_ctx =
      kms_base_sdp_endpoint_get_first_negotiated_sdp_ctx (base_endpoint);
  const GSList *item;

  if (neg_sdp_ctx == NULL) {
    GST_WARNING_OBJECT (self, "Negotiated session not set");
    return FALSE;
  }

  item = kms_sdp_message_context_get_medias (neg_sdp_ctx);
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
kms_base_rtp_endpoint_update_stats (KmsBaseRtpEndpoint * self,
    GstElement * depayloader, KmsMediaType media)
{
  KmsStatsProbe *probe;
  GstPad *pad;

  pad = gst_element_get_static_pad (depayloader, "sink");
  probe = kms_stats_probe_new (pad, media);
  g_object_unref (pad);

  g_mutex_lock (&self->priv->stats.mutex);

  if (self->priv->stats.enabled) {
    kms_stats_probe_latency_meta_set_valid (probe, TRUE);
  }

  self->priv->stats.probes = g_slist_prepend (self->priv->stats.probes, probe);

  g_mutex_unlock (&self->priv->stats.mutex);
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
    kms_base_rtp_endpoint_update_stats (self, depayloader, media);
    gst_bin_add (GST_BIN (self), depayloader);
    gst_element_link_pads (depayloader, "src", agnostic, "sink");
    gst_element_link_pads (rtpbin, GST_OBJECT_NAME (pad), depayloader, "sink");
    gst_element_sync_state_with_parent (depayloader);
  } else {
    GstElement *fake = gst_element_factory_make ("fakesink", NULL);

    g_object_set (fake, "async", FALSE, "sync", FALSE, NULL);

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
    GstPadProbeInfo * info, gpointer user_data)
{
  GstElement *jitterbuffer = GST_PAD_PARENT (pad);
  gint latency = GPOINTER_TO_INT (user_data);

  GST_DEBUG_OBJECT (jitterbuffer, "Setting latency to: %d", latency);
  g_object_set (jitterbuffer, "latency", latency, NULL);

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
      kms_base_rtp_endpoint_change_latency_probe,
      GINT_TO_POINTER (session ==
          VIDEO_RTP_SESSION ? JB_READY_VIDEO_LATENCY : JB_READY_AUDIO_LATENCY),
      NULL);
  g_object_unref (src_pad);

  g_mutex_lock (&self->priv->stats.mutex);

  rtp_stats =
      g_hash_table_lookup (self->priv->stats.rtp_stats,
      GUINT_TO_POINTER (session));

  if (rtp_stats != NULL) {
    ssrc_stats = ssrc_stats_new (ssrc, jitterbuffer);
    rtp_stats->ssrcs = g_slist_prepend (rtp_stats->ssrcs, ssrc_stats);
  } else {
    GST_ERROR_OBJECT (self, "Session %u exists for SSRC %u", session, ssrc);
  }

  g_mutex_unlock (&self->priv->stats.mutex);

  if (session == VIDEO_RTP_SESSION) {
    gboolean rtcp_nack = kms_base_rtp_endpoint_is_video_rtcp_nack (self);

    g_object_set (jitterbuffer, "do-lost", TRUE,
        "do-retransmission", rtcp_nack, "rtx-next-seqnum", FALSE, NULL);
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
set_outbound_additional_params (const GstStructure * session_stats,
    const gchar * ssrc_id, guint rtt, guint fraction_lost, gint packet_lost)
{
  const GstStructure *ssrc_stats;

  ssrc_stats = get_structure_from_id (session_stats, ssrc_id);

  if (ssrc_stats == NULL) {
    return;
  }

  gst_structure_set ((GstStructure *) ssrc_stats, "round-trip-time",
      G_TYPE_UINT, rtt, "outbound-fraction-lost", G_TYPE_UINT, fraction_lost,
      "outbound-packet-lost", G_TYPE_INT, packet_lost, NULL);
}

static gboolean
filter_rtp_source (GstSDPDirection direction, gboolean internal)
{
  switch (direction) {
    case SENDONLY:
      /* filter non internal sources */
      return !internal;
    case RECVONLY:
      /* filter internal sources */
      return internal;
    case SENDRECV:
      return FALSE;
    default:
      return TRUE;
  }
}

static void
append_rtp_session_stats (gpointer * session, KmsRTPSessionStats * rtp_stats,
    GstStructure * stats)
{
  GstStructure *session_stats;
  gchar *str_session;
  GValueArray *arr;
  gchar *ssrc_id = NULL;
  guint i, f_lost, rtt;
  gint p_lost;

  p_lost = f_lost = rtt = 0;

  g_object_get (rtp_stats->rtp_session, "stats", &session_stats, NULL);

  if (session_stats == NULL)
    return;

  /* Get stats for each source */
  g_object_get (rtp_stats->rtp_session, "sources", &arr, NULL);

  for (i = 0; i < arr->n_values; i++) {
    GstElement *jitter_buffer;
    GstStructure *ssrc_stats;
    gboolean internal;
    GObject *source;
    GValue *val;
    gchar *name;
    guint ssrc;
    gchar *id;

    val = g_value_array_get_nth (arr, i);
    source = g_value_get_object (val);

    g_object_get (source, "stats", &ssrc_stats, "ssrc", &ssrc, NULL);
    gst_structure_get (ssrc_stats, "internal", G_TYPE_BOOLEAN, &internal, NULL);

    name = g_strdup_printf ("ssrc-%u", ssrc);

    if (internal) {
      if (ssrc_id == NULL) {
        ssrc_id = g_strdup (name);
      } else {
        GST_WARNING ("Session %d has more than 1 internal source",
            GPOINTER_TO_UINT (session));
      }
    } else {
      gst_structure_get (ssrc_stats, "rb-round-trip", G_TYPE_UINT, &rtt,
          "rb-fractionlost", G_TYPE_UINT, &f_lost, "rb-packetslost", G_TYPE_INT,
          &p_lost, NULL);
    }

    if (filter_rtp_source (rtp_stats->direction, internal)) {
      gst_structure_free (ssrc_stats);
      g_free (name);
      continue;
    }

    id = g_object_get_data (source, KMS_KEY_ID);

    if (id == NULL) {
      assign_uuid (source);
      id = g_object_get_data (source, KMS_KEY_ID);
    }

    gst_structure_set (ssrc_stats, "id", G_TYPE_STRING, id, NULL);

    jitter_buffer = rtp_session_stats_get_jitter_buffer (rtp_stats, ssrc);

    if (jitter_buffer != NULL) {
      ssrc_stats_add_jitter_stats (ssrc_stats, jitter_buffer);
    }

    gst_structure_set (session_stats, name, GST_TYPE_STRUCTURE, ssrc_stats,
        NULL);

    gst_structure_free (ssrc_stats);
    g_free (name);
  }

  if (ssrc_id != NULL) {
    set_outbound_additional_params (session_stats, ssrc_id, rtt, f_lost,
        p_lost);
    g_free (ssrc_id);
  }

  g_value_array_free (arr);

  str_session = g_strdup_printf ("session-%u", GPOINTER_TO_UINT (session));
  gst_structure_set (stats, str_session, GST_TYPE_STRUCTURE, session_stats,
      NULL);

  gst_structure_free (session_stats);
  g_free (str_session);
}

static GstStructure *
kms_base_rtp_endpoint_add_rtp_stats (KmsBaseRtpEndpoint * self,
    GstStructure * stats, const gchar * selector)
{
  KmsRTPSessionStats *rtp_stats;
  guint session_id;

  g_mutex_lock (&self->priv->stats.mutex);

  if (selector == NULL) {
    /* No selector provided. All stats will be generated */
    g_hash_table_foreach (self->priv->stats.rtp_stats,
        (GHFunc) append_rtp_session_stats, stats);
    goto end;
  }

  if (g_strcmp0 (selector, AUDIO_STREAM_NAME) == 0) {
    session_id = AUDIO_RTP_SESSION;
  } else if (g_strcmp0 (selector, VIDEO_STREAM_NAME) == 0) {
    session_id = VIDEO_RTP_SESSION;
  } else {
    GST_WARNING_OBJECT (self, "Invalid selector provided: %s", selector);
    goto end;
  }

  rtp_stats = g_hash_table_lookup (self->priv->stats.rtp_stats,
      GUINT_TO_POINTER (session_id));

  if (rtp_stats == NULL) {
    GST_DEBUG_OBJECT (self, "No available stats for '%s'", selector);
    goto end;
  }

  append_rtp_session_stats (GUINT_TO_POINTER (session_id), rtp_stats, stats);

end:
  g_mutex_unlock (&self->priv->stats.mutex);

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
    case PROP_MIN_VIDEO_RECV_BW:{
      int max_recv_bw;
      guint v = g_value_get_uint (value);

      g_object_get (self, "max-video-recv-bandwidth", &max_recv_bw, NULL);

      if (max_recv_bw != 0 && v > max_recv_bw) {
        v = max_recv_bw;
        GST_WARNING_OBJECT (object,
            "Trying to set min > max. Setting %" G_GUINT32_FORMAT, v);
      }

      self->priv->min_video_recv_bw = v;
      break;
    }
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
    case PROP_REMB_PARAMS:
      if (self->priv->rl != NULL) {
        GstStructure *params = g_value_get_boxed (value);

        GST_DEBUG_OBJECT (self,
            "Set to already created RembLocal and RembRemote");
        kms_remb_local_set_params (self->priv->rl, params);
        kms_remb_remote_set_params (self->priv->rm, params);
      } else {
        GST_DEBUG_OBJECT (self, "Set to aux structure");
        if (self->priv->remb_params != NULL) {
          gst_structure_free (self->priv->remb_params);
        }
        self->priv->remb_params = g_value_dup_boxed (value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_rtp_endpoint_get_property (GObject * object, guint property_id,
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
    case PROP_MIN_VIDEO_RECV_BW:
      g_value_set_uint (value, self->priv->min_video_recv_bw);
    case PROP_MIN_VIDEO_SEND_BW:
      g_value_set_uint (value, self->priv->min_video_send_bw);
      break;
    case PROP_MAX_VIDEO_SEND_BW:
      g_value_set_uint (value, self->priv->max_video_send_bw);
      break;
    case PROP_MEDIA_STATE:
      g_value_set_enum (value, self->priv->media_state);
      break;
    case PROP_REMB_PARAMS:
      if (self->priv->rl != NULL) {
        GstStructure *params = gst_structure_new_empty ("remb-params");

        GST_DEBUG_OBJECT (self,
            "Get from already created RembLocal and RembRemote");
        kms_remb_local_get_params (self->priv->rl, &params);
        kms_remb_remote_get_params (self->priv->rm, &params);
        g_value_take_boxed (value, params);
      } else if (self->priv->remb_params != NULL) {
        GST_DEBUG_OBJECT (self, "Get from aux structure");
        g_value_set_boxed (value, self->priv->remb_params);
      }
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
kms_base_rtp_endpoint_destroy_stats (KmsBaseRtpEndpoint * self)
{
  g_mutex_clear (&self->priv->stats.mutex);
  g_hash_table_destroy (self->priv->stats.rtp_stats);
  g_slist_free_full (self->priv->stats.probes,
      (GDestroyNotify) kms_stats_probe_destroy);
}

static void
kms_base_rtp_endpoint_enable_connections_stats (gpointer key, gpointer value,
    gpointer user_data)
{
  kms_base_rtp_session_enable_connections_stats (KMS_BASE_RTP_SESSION (value));
}

static void
kms_base_rtp_endpoint_disable_connections_stats (gpointer key, gpointer value,
    gpointer user_data)
{
  kms_base_rtp_session_disable_connections_stats (KMS_BASE_RTP_SESSION (value));
}

static void
kms_base_rtp_endpoint_finalize (GObject * gobject)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (gobject);
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  GHashTable *sessions;

  GST_DEBUG_OBJECT (self, "finalize");

  kms_base_rtp_endpoint_destroy_stats (self);

  if (self->priv->remb_params != NULL) {
    gst_structure_free (self->priv->remb_params);
  }

  kms_remb_local_destroy (self->priv->rl);
  kms_remb_remote_destroy (self->priv->rm);

  sessions = kms_base_sdp_endpoint_get_sessions (base_endpoint);
  g_hash_table_foreach (sessions,
      kms_base_rtp_endpoint_disable_connections_stats, NULL);

  G_OBJECT_CLASS (kms_base_rtp_endpoint_parent_class)->finalize (gobject);
}

typedef struct _KmsRembStats
{
  GstStructure *stats;
  guint session;
} KmsRembStats;

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

static GstStructure *
kms_base_rtp_endpoint_stats (KmsElement * obj, gchar * selector)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (obj);
  GstStructure *stats, *rtp_stats, *e_stats;
  gboolean enabled;

  /* chain up */
  stats =
      KMS_ELEMENT_CLASS (kms_base_rtp_endpoint_parent_class)->stats (obj,
      selector);

  rtp_stats = gst_structure_new_empty (KMS_RTP_STRUCT_NAME);
  kms_base_rtp_endpoint_add_rtp_stats (self, rtp_stats, selector);
  kms_base_rtp_endpoint_append_remb_stats (self, rtp_stats);
  gst_structure_set (stats, KMS_RTC_STATISTICS_FIELD, GST_TYPE_STRUCTURE,
      rtp_stats, NULL);

  g_object_get (self, "media-stats", &enabled, NULL);

  if (!enabled) {
    return stats;
  }

  e_stats = kms_stats_get_element_stats (stats);

  if (e_stats == NULL) {
    return stats;
  }

  /* Video and audio latencies are avery small values in */
  /* nano seconds so there is no harm in casting them to */
  /* uint64 even we might lose a bit of preccision.      */

  gst_structure_set (e_stats, "video-e2e-latency", G_TYPE_UINT64,
      (guint64) self->priv->stats.vi, "audio-e2e-latency", G_TYPE_UINT64,
      (guint64) self->priv->stats.ai, NULL);

  return stats;
}

static void
kms_base_rtp_endpoint_enable_media_stats (KmsStatsProbe * probe,
    KmsBaseRtpEndpoint * self)
{
  kms_stats_probe_latency_meta_set_valid (probe, TRUE);
}

static void
kms_base_rtp_endpoint_disable_media_stats (KmsStatsProbe * probe,
    KmsBaseRtpEndpoint * self)
{
  kms_stats_probe_remove (probe);
}

static void
kms_base_rtp_endpoint_collect_media_stats (KmsElement * obj, gboolean enable)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (obj);
  KmsBaseSdpEndpoint *base_endpoint = KMS_BASE_SDP_ENDPOINT (self);
  GHashTable *sessions;

  KMS_ELEMENT_LOCK (self);
  sessions = kms_base_sdp_endpoint_get_sessions (base_endpoint);

  g_mutex_lock (&self->priv->stats.mutex);

  self->priv->stats.enabled = enable;

  if (enable) {
    g_slist_foreach (self->priv->stats.probes,
        (GFunc) kms_base_rtp_endpoint_enable_media_stats, self);
    g_hash_table_foreach (sessions,
        kms_base_rtp_endpoint_enable_connections_stats, NULL);
  } else {
    g_slist_foreach (self->priv->stats.probes,
        (GFunc) kms_base_rtp_endpoint_disable_media_stats, self);
    g_hash_table_foreach (sessions,
        kms_base_rtp_endpoint_disable_connections_stats, NULL);
  }

  g_mutex_unlock (&self->priv->stats.mutex);
  KMS_ELEMENT_UNLOCK (self);

  KMS_ELEMENT_CLASS
      (kms_base_rtp_endpoint_parent_class)->collect_media_stats (obj, enable);
}

static void
kms_base_rtp_endpoint_class_init (KmsBaseRtpEndpointClass * klass)
{
  KmsBaseSdpEndpointClass *base_endpoint_class;
  GstElementClass *gstelement_class;
  KmsElementClass *kmselement_class;
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = kms_base_rtp_endpoint_dispose;
  object_class->finalize = kms_base_rtp_endpoint_finalize;
  object_class->set_property = kms_base_rtp_endpoint_set_property;
  object_class->get_property = kms_base_rtp_endpoint_get_property;

  kmselement_class = KMS_ELEMENT_CLASS (klass);
  kmselement_class->stats = GST_DEBUG_FUNCPTR (kms_base_rtp_endpoint_stats);
  kmselement_class->collect_media_stats =
      GST_DEBUG_FUNCPTR (kms_base_rtp_endpoint_collect_media_stats);

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "BaseRtpEndpoint",
      "Base/Bin/BaseRtpEndpoints",
      "Base class for RtpEndpoints",
      "José Antonio Santos Cadenas <santoscadenas@kurento.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  klass->get_connection_state = kms_base_rtp_endpoint_get_connection_state;
  klass->request_local_key_frame =
      kms_base_rtp_endpoint_request_local_key_frame;

  base_endpoint_class = KMS_BASE_SDP_ENDPOINT_CLASS (klass);
  base_endpoint_class->create_session_internal =
      kms_base_rtp_endpoint_create_session_internal;
  base_endpoint_class->start_transport_send =
      kms_base_rtp_endpoint_start_transport_send;
  base_endpoint_class->connect_input_elements =
      kms_base_rtp_endpoint_connect_input_elements;

  /* Media handler management */
  base_endpoint_class->create_media_handler = kms_base_rtp_create_media_handler;

  base_endpoint_class->configure_media = kms_base_rtp_endpoint_configure_media;

  g_object_class_install_property (object_class, PROP_MEDIA_STATE,
      g_param_spec_enum ("media-state", "Media state", "Media state",
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

  g_object_class_install_property (object_class, PROP_MIN_VIDEO_RECV_BW,
      g_param_spec_uint ("min-video-recv-bandwidth",
          "Minimum video bandwidth for receiving",
          "Minimum video bandwidth for receiving. Unit: kbps(kilobits per second). 0: unlimited",
          0, G_MAXUINT32, MIN_VIDEO_RECV_BW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

  g_object_class_install_property (object_class, PROP_REMB_PARAMS,
      g_param_spec_boxed ("remb-params", "remb params",
          "Set parameters for REMB algorithm",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* set signals */
  obj_signals[GET_CONNECTION_STATE] =
      g_signal_new ("get-connection_state",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, get_connection_state), NULL,
      NULL, __kms_core_marshal_ENUM__STRING, KMS_TYPE_CONNECTION_STATE, 1,
      G_TYPE_STRING);

  obj_signals[CONNECTION_STATE_CHANGED] =
      g_signal_new ("connection-state-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseRtpEndpointClass, connection_state_changed), NULL,
      NULL, __kms_core_marshal_VOID__STRING_ENUM, G_TYPE_NONE, 2, G_TYPE_STRING,
      KMS_TYPE_CONNECTION_STATE);

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

  if (self->priv->media_state != new_state) {
    GST_DEBUG_OBJECT (self, "Media state changed to '%d'", new_state);
    self->priv->media_state = new_state;
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
kms_base_rtp_endpoint_init_stats (KmsBaseRtpEndpoint * self)
{
  self->priv->stats.enabled = FALSE;
  self->priv->stats.rtp_stats = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, (GDestroyNotify) rtp_session_stats_destroy);
  g_mutex_init (&self->priv->stats.mutex);

  self->priv->stats.ai = 0.0;
  self->priv->stats.vi = 0.0;
}

static void
kms_base_rtp_endpoint_init (KmsBaseRtpEndpoint * self)
{
  self->priv = KMS_BASE_RTP_ENDPOINT_GET_PRIVATE (self);
  self->priv->rtcp_mux = DEFAULT_RTCP_MUX;
  self->priv->rtcp_nack = DEFAULT_RTCP_NACK;
  self->priv->rtcp_remb = DEFAULT_RTCP_REMB;

  self->priv->min_video_recv_bw = MIN_VIDEO_RECV_BW_DEFAULT;
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
  g_signal_connect (self->priv->rtpbin, "new-jitterbuffer",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_new_jitterbuffer), self);

  g_signal_connect (self->priv->rtpbin, "on-timeout",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_timeout), self);

  g_signal_connect (self->priv->rtpbin, "on-ssrc-active",
      G_CALLBACK (kms_base_rtp_endpoint_rtpbin_on_ssrc_active), self);

  g_object_set (self, "accept-eos", FALSE, NULL);

  gst_bin_add (GST_BIN (self), self->priv->rtpbin);

  self->priv->audio_payloader = NULL;
  self->priv->video_payloader = NULL;

  self->priv->audio_payloader_connected = FALSE;
  self->priv->video_payloader_connected = FALSE;

  kms_base_rtp_endpoint_init_stats (self);
}

static void
kms_i_rtp_session_manager_interface_init (KmsIRtpSessionManagerInterface *
    iface)
{
  iface->request_rtp_sink = kms_base_rtp_endpoint_request_rtp_sink;
  iface->request_rtp_src = kms_base_rtp_endpoint_request_rtp_src;
  iface->request_rtcp_sink = kms_base_rtp_endpoint_request_rtcp_sink;
  iface->request_rtcp_src = kms_base_rtp_endpoint_request_rtcp_src;
}
