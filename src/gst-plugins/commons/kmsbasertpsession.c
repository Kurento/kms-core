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
#  include <config.h>
#endif

#include "kmsbasertpsession.h"
#include "constants.h"
#include "kmsutils.h"
#include "sdp_utils.h"

#define GST_DEFAULT_NAME "kmsbasertpsession"
#define GST_CAT_DEFAULT kms_base_rtp_session_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_base_rtp_session_parent_class parent_class
G_DEFINE_TYPE (KmsBaseRtpSession, kms_base_rtp_session, KMS_TYPE_SDP_SESSION);

#define BUNDLE_CONN_ADDED "bundle-conn-added"
#define RTCP_DEMUX_PEER "rtcp-demux-peer"

KmsBaseRtpSession *
kms_base_rtp_session_new (KmsBaseSdpEndpoint * ep, guint id,
    KmsIRtpSessionManager * manager)
{
  GObject *obj;
  KmsBaseRtpSession *self;

  obj = g_object_new (KMS_TYPE_BASE_RTP_SESSION, NULL);
  self = KMS_BASE_RTP_SESSION (obj);
  KMS_BASE_RTP_SESSION_CLASS (G_OBJECT_GET_CLASS (self))->post_constructor
      (self, ep, id, manager);

  return self;
}

KmsIRtpConnection *
kms_base_rtp_session_get_connection_by_name (KmsBaseRtpSession * self,
    const gchar * name)
{
  gpointer *conn;

  conn = g_hash_table_lookup (self->conns, name);
  if (conn == NULL) {
    return NULL;
  }

  return KMS_I_RTP_CONNECTION (conn);
}

KmsIRtpConnection *
kms_base_rtp_session_get_connection (KmsBaseRtpSession * self,
    SdpMediaConfig * mconf)
{
  gchar *name = kms_utils_create_connection_name_from_media_config (mconf);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_session_get_connection_by_name (self, name);
  if (conn == NULL) {
    GST_WARNING_OBJECT (self, "Connection '%s' not found", name);
    g_free (name);
    return NULL;
  }
  g_free (name);

  return conn;
}

/* Start Transport Send begin */

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
kms_base_rtp_session_link_pads (GstPad * src, GstPad * sink)
{
  if (gst_pad_link_full (src, sink, GST_PAD_LINK_CHECK_CAPS) != GST_PAD_LINK_OK) {
    GST_ERROR ("Error linking pads (src: %" GST_PTR_FORMAT ", sink: %"
        GST_PTR_FORMAT ")", src, sink);
  }
}

static void
rtp_ssrc_demux_new_ssrc_pad (GstElement * ssrcdemux, guint ssrc, GstPad * pad,
    KmsBaseRtpSession * self)
{
  KmsSdpSession *sdp_sess = KMS_SDP_SESSION (self);
  const gchar *rtp_pad_name = GST_OBJECT_NAME (pad);
  gchar *rtcp_pad_name;
  SdpMediaConfig *mconf;
  GstPad *src, *sink;

  GST_DEBUG_OBJECT (self, "pad: %" GST_PTR_FORMAT " ssrc: %" G_GUINT32_FORMAT,
      pad, ssrc);

  /* inmediate-TODO: lock per session */
  KMS_ELEMENT_LOCK (sdp_sess->ep);

  if (self->remote_audio_ssrc == ssrc
      || ssrcs_are_mapped (ssrcdemux, self->local_audio_ssrc, ssrc)) {
    mconf = self->audio_neg_mconf;
  } else if (self->remote_video_ssrc == ssrc
      || ssrcs_are_mapped (ssrcdemux, self->local_video_ssrc, ssrc)) {
    mconf = self->video_neg_mconf;
  } else {
    GST_ERROR_OBJECT (pad, "SSRC %" G_GUINT32_FORMAT " not matching.", ssrc);
    goto end;
  }

  /* RTP */
  sink =
      kms_i_rtp_session_manager_request_rtp_sink (self->manager, self, mconf);
  kms_base_rtp_session_link_pads (pad, sink);
  g_object_unref (sink);

  /* RTCP */
  rtcp_pad_name = g_strconcat ("rtcp_", rtp_pad_name, NULL);
  src = gst_element_get_static_pad (ssrcdemux, rtcp_pad_name);
  g_free (rtcp_pad_name);
  sink =
      kms_i_rtp_session_manager_request_rtcp_sink (self->manager, self, mconf);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

end:
  KMS_ELEMENT_UNLOCK (sdp_sess->ep);
}

static void
kms_base_rtp_session_add_bundle_connection (KmsBaseRtpSession * self,
    KmsIRtpConnection * conn, SdpMediaConfig * mconf, gboolean active)
{
  gboolean added;
  GstElement *ssrcdemux;
  GstElement *rtcpdemux;        /* FIXME: Useful for local and remote ssrcs mapping */
  GstPad *src, *sink;

  if (GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (conn), BUNDLE_CONN_ADDED))) {
    GST_DEBUG_OBJECT (self, "Connection configured");
    return;
  }

  g_object_set_data (G_OBJECT (conn), BUNDLE_CONN_ADDED,
      GUINT_TO_POINTER (TRUE));

  g_object_get (conn, "added", &added, NULL);
  if (!added) {
    kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  }

  ssrcdemux = gst_element_factory_make ("rtpssrcdemux", NULL);
  rtcpdemux = gst_element_factory_make ("rtcpdemux", NULL);

  g_object_set_data_full (G_OBJECT (ssrcdemux), RTCP_DEMUX_PEER,
      g_object_ref (rtcpdemux), g_object_unref);
  g_signal_connect (ssrcdemux, "new-ssrc-pad",
      G_CALLBACK (rtp_ssrc_demux_new_ssrc_pad), self);

  kms_i_rtp_connection_sink_sync_state_with_parent (conn);
  gst_bin_add_many (GST_BIN (self), ssrcdemux, rtcpdemux, NULL);

  /* RTP */
  src = kms_i_rtp_connection_request_rtp_src (conn);
  sink = gst_element_get_static_pad (ssrcdemux, "sink");
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  src = kms_i_rtp_connection_request_rtcp_src (conn);
  sink = gst_element_get_static_pad (rtcpdemux, "sink");
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
  gst_element_link_pads (rtcpdemux, "rtcp_src", ssrcdemux, "rtcp_sink");

  gst_element_sync_state_with_parent_target_state (ssrcdemux);
  gst_element_sync_state_with_parent_target_state (rtcpdemux);

  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

/* inmediate-TODO: add when conn is "connected" */
static void
kms_base_rtp_session_add_connection_sink (KmsBaseRtpSession * self,
    KmsIRtpConnection * conn, SdpMediaConfig * mconf)
{
  GstPad *src, *sink;

  /* RTP */
  src = kms_i_rtp_session_manager_request_rtp_src (self->manager, self, mconf);
  sink = kms_i_rtp_connection_request_rtp_sink (conn);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  src = kms_i_rtp_session_manager_request_rtcp_src (self->manager, self, mconf);
  sink = kms_i_rtp_connection_request_rtcp_sink (conn);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
}

static void
kms_base_rtp_session_add_connection_src (KmsBaseRtpSession * self,
    KmsIRtpConnection * conn, SdpMediaConfig * mconf)
{
  GstPad *src, *sink;

  /* RTP */
  sink =
      kms_i_rtp_session_manager_request_rtp_sink (self->manager, self, mconf);
  src = kms_i_rtp_connection_request_rtp_src (conn);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  sink =
      kms_i_rtp_session_manager_request_rtcp_sink (self->manager, self, mconf);
  src = kms_i_rtp_connection_request_rtcp_src (conn);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);
}

static void
kms_base_rtp_session_add_rtcp_mux_connection (KmsBaseRtpSession * self,
    KmsIRtpConnection * conn, SdpMediaConfig * mconf, gboolean active)
{
  /* FIXME: Useful for local and remote ssrcs mapping */
  GstElement *rtcpdemux = gst_element_factory_make ("rtcpdemux", NULL);
  GstPad *src, *sink;

  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);
  gst_bin_add (GST_BIN (self), rtcpdemux);

  /* RTP */
  src = kms_i_rtp_connection_request_rtp_src (conn);
  sink =
      kms_i_rtp_session_manager_request_rtp_sink (self->manager, self, mconf);
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  /* RTCP */
  src = gst_element_get_static_pad (rtcpdemux, "rtcp_src");
  sink =
      kms_i_rtp_session_manager_request_rtcp_sink (self->manager, self, mconf);
  g_object_unref (src);
  g_object_unref (sink);

  src = kms_i_rtp_connection_request_rtcp_src (conn);
  sink = gst_element_get_static_pad (rtcpdemux, "sink");
  kms_base_rtp_session_link_pads (src, sink);
  g_object_unref (src);
  g_object_unref (sink);

  gst_element_sync_state_with_parent_target_state (rtcpdemux);
  kms_base_rtp_session_add_connection_sink (self, conn, mconf);

  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

static void
kms_base_rtp_session_add_connection (KmsBaseRtpSession * self,
    KmsIRtpConnection * conn, SdpMediaConfig * mconf, gboolean active)
{
  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);

  kms_base_rtp_session_add_connection_sink (self, conn, mconf);
  kms_base_rtp_session_add_connection_src (self, conn, mconf);

  kms_i_rtp_connection_src_sync_state_with_parent (conn);
}

static gboolean
kms_base_rtp_session_add_connection_for_session (KmsBaseRtpSession * self,
    SdpMediaConfig * mconf, gboolean active)
{
  KmsIRtpConnection *conn;
  SdpMediaGroup *group = kms_sdp_media_config_get_group (mconf);

  conn = kms_base_rtp_session_get_connection (self, mconf);
  if (conn == NULL) {
    return FALSE;
  }

  if (group != NULL) {          /* bundle */
    kms_base_rtp_session_add_bundle_connection (self, conn, mconf, active);
    kms_base_rtp_session_add_connection_sink (self, conn, mconf);
  } else if (kms_sdp_media_config_is_rtcp_mux (mconf)) {
    kms_base_rtp_session_add_rtcp_mux_connection (self, conn, mconf, active);
  } else {
    kms_base_rtp_session_add_connection (self, conn, mconf, active);
  }

  return TRUE;
}

static const gchar *
kms_base_rtp_session_process_remote_ssrc (KmsBaseRtpSession * self,
    GstSDPMedia * remote_media, SdpMediaConfig * neg_mconf)
{
  const gchar *media_str = gst_sdp_media_get_media (remote_media);

  if (g_strcmp0 (AUDIO_STREAM_NAME, media_str) == 0) {
    guint ssrc = sdp_utils_media_get_ssrc (remote_media);

    GST_DEBUG_OBJECT (self, "Add remote audio ssrc: %u", ssrc);
    self->remote_audio_ssrc = ssrc;
    self->audio_neg_mconf = neg_mconf;

    return AUDIO_RTP_SESSION_STR;
  } else if (g_strcmp0 (VIDEO_STREAM_NAME, media_str) == 0) {
    guint ssrc = sdp_utils_media_get_ssrc (remote_media);

    GST_DEBUG_OBJECT (self, "Add remote video ssrc: %u", ssrc);
    self->remote_video_ssrc = ssrc;
    self->video_neg_mconf = neg_mconf;

    return VIDEO_RTP_SESSION_STR;
  }

  GST_WARNING_OBJECT (self, "Media '%s' not supported", media_str);

  return NULL;
}

static gboolean
kms_base_rtp_session_configure_connection (KmsBaseRtpSession * self,
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
    GST_DEBUG_OBJECT (self, "'%s' protocol does not need RTP connection",
        neg_proto_str);
    /* It cannot be managed here but could be managed by the child class */
    return FALSE;
  }

  if (g_strcmp0 (neg_media_str, remote_media_str) != 0) {
    GST_WARNING_OBJECT (self,
        "Negotiated media ('%s') not matching with remote media ('%s')",
        neg_media_str, remote_media_str);
    return FALSE;
  }

  /* inmediate-TODO: rtp_session_str should not be needed */
  rtp_session_str =
      kms_base_rtp_session_process_remote_ssrc (self, remote_media, neg_mconf);

  if (rtp_session_str == NULL) {
    return TRUE;                /* It cannot be managed here but could be managed by the child class */
  }

  active = sdp_utils_media_is_active (neg_media, offerer);

  return kms_base_rtp_session_add_connection_for_session (self, neg_mconf,
      active);
}

/* inmediate-TODO: check relationship with kms_webrtc_endpoint_start_transport_send*/
void
kms_base_rtp_session_start_transport_send (KmsBaseRtpSession * self,
    gboolean offerer)
{
  KmsSdpSession *sdp_sess = KMS_SDP_SESSION (self);
  GSList *item = kms_sdp_message_context_get_medias (sdp_sess->neg_sdp_ctx);
  GSList *remote_media_list =
      kms_sdp_message_context_get_medias (sdp_sess->remote_sdp_ctx);

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

    if (!kms_base_rtp_session_configure_connection (self, neg_mconf,
            remote_mconf, offerer)) {
      GST_WARNING_OBJECT (self, "Cannot configure connection for media %d.",
          mid);
    }
  }
}

/* Start Transport Send end */

static void
kms_base_rtp_session_finalize (GObject * object)
{
  KmsBaseRtpSession *self = KMS_BASE_RTP_SESSION (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_hash_table_destroy (self->conns);

  /* chain up */
  G_OBJECT_CLASS (kms_base_rtp_session_parent_class)->finalize (object);
}

static void
kms_base_rtp_session_post_constructor (KmsBaseRtpSession * self,
    KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager)
{
  KmsSdpSession *sdp_sess = KMS_SDP_SESSION (self);

  self->manager = manager;
  KMS_SDP_SESSION_CLASS
      (kms_base_rtp_session_parent_class)->post_constructor (sdp_sess, ep, id);
}

static void
kms_base_rtp_session_init (KmsBaseRtpSession * self)
{
  self->conns =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
kms_base_rtp_session_class_init (KmsBaseRtpSessionClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gobject_class->finalize = kms_base_rtp_session_finalize;

  klass->post_constructor = kms_base_rtp_session_post_constructor;

  gst_element_class_set_details_simple (gstelement_class,
      "BaseRtpSession",
      "Generic",
      "Base bin to manage elements related with a RTP session.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");
}
