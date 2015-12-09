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

#ifndef __KMS_BASE_RTP_SESSION_H__
#define __KMS_BASE_RTP_SESSION_H__

#include <gst/gst.h>
#include "kmssdpsession.h"
#include "kmsirtpsessionmanager.h"
#include "kmsirtpconnection.h"
#include "kmsconnectionstate.h"

G_BEGIN_DECLS

typedef struct _KmsIRtpSessionManager KmsIRtpSessionManager;

/* #defines don't like whitespacey bits */
#define KMS_TYPE_BASE_RTP_SESSION \
  (kms_base_rtp_session_get_type())
#define KMS_BASE_RTP_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_BASE_RTP_SESSION,KmsBaseRtpSession))
#define KMS_BASE_RTP_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_BASE_RTP_SESSION,KmsBaseRtpSessionClass))
#define KMS_IS_BASE_RTP_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_BASE_RTP_SESSION))
#define KMS_IS_BASE_RTP_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_BASE_RTP_SESSION))
#define KMS_BASE_RTP_SESSION_CAST(obj) ((KmsBaseRtpSession*)(obj))

typedef struct _KmsBaseRtpSession KmsBaseRtpSession;
typedef struct _KmsBaseRtpSessionClass KmsBaseRtpSessionClass;
typedef struct _KmsBaseRTPSessionStats KmsBaseRTPSessionStats;

struct _KmsBaseRtpSession
{
  KmsSdpSession parent;

  KmsIRtpSessionManager *manager;
  GHashTable *conns;
  KmsConnectionState conn_state;

  SdpMediaConfig *audio_neg_mconf;
  guint32 local_audio_ssrc;
  guint32 remote_audio_ssrc;

  SdpMediaConfig *video_neg_mconf;
  guint32 local_video_ssrc;
  guint32 remote_video_ssrc;

  KmsBaseRTPSessionStats *stats;
};

struct _KmsBaseRtpSessionClass
{
  KmsSdpSessionClass parent_class;

  /* private */
  /* virtual methods */
  void (*post_constructor) (KmsBaseRtpSession * self, KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager);

  KmsIRtpConnection * (*create_connection) (KmsBaseRtpSession *self, SdpMediaConfig * mconf, const gchar *name, guint16 min_port, guint16 max_port);
  KmsIRtcpMuxConnection* (*create_rtcp_mux_connection) (KmsBaseRtpSession *self, const gchar *name, guint16 min_port, guint16 max_port);
  KmsIBundleConnection * (*create_bundle_connection) (KmsBaseRtpSession *self, const gchar *name, guint16 min_port, guint16 max_port);

  void (*connection_state_changed) (KmsBaseRtpSession * self, KmsConnectionState new_state);
};

GType kms_base_rtp_session_get_type (void);

KmsBaseRtpSession * kms_base_rtp_session_new (KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager);
KmsIRtpConnection * kms_base_rtp_session_get_connection_by_name (KmsBaseRtpSession * self, const gchar * name);
KmsIRtpConnection * kms_base_rtp_session_get_connection (KmsBaseRtpSession * self, SdpMediaConfig * mconf);
KmsIRtpConnection * kms_base_rtp_session_create_connection (KmsBaseRtpSession * self, SdpMediaConfig * mconf, guint16 min_port, guint16 max_port);

void kms_base_rtp_session_start_transport_send (KmsBaseRtpSession * self, gboolean offerer);

void kms_base_rtp_session_enable_connections_stats (KmsBaseRtpSession * self);
void kms_base_rtp_session_disable_connections_stats (KmsBaseRtpSession * self);

G_END_DECLS
#endif /* __KMS_BASE_RTP_SESSION_H__ */
