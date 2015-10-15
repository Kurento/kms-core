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

#ifndef __KMS_I_RTP_SESSION_MANAGER_H__
#define __KMS_I_RTP_SESSION_MANAGER_H__

#include <gst/gst.h>
#include "kmsbasertpsession.h"

G_BEGIN_DECLS

typedef struct _KmsBaseRtpSession KmsBaseRtpSession;

/* KmsIRtpSessionManager begin */
#define KMS_TYPE_I_RTP_SESSION_MANAGER \
  (kms_i_rtp_session_manager_get_type())
#define KMS_I_RTP_SESSION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_I_RTP_SESSION_MANAGER,KmsIRtpSessionManager))
#define KMS_IS_I_RTP_SESSION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_I_RTP_SESSION_MANAGER))
#define KMS_I_RTP_SESSION_MANAGER_CAST(obj) ((KmsIRtpSessionManager*)(obj))
#define KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj),KMS_TYPE_I_RTP_SESSION_MANAGER,KmsIRtpSessionManagerInterface))
#define KMS_I_RTP_SESSION_MANAGER_INTERFACE(iface) ((KmsIRtpSessionManagerInterface *) iface)

typedef struct _KmsIRtpSessionManager KmsIRtpSessionManager;
typedef struct _KmsIRtpSessionManagerInterface KmsIRtpSessionManagerInterface;

struct _KmsIRtpSessionManagerInterface
{
  GTypeInterface parent;

  /* virtual methods */
  GstPad * (*request_rtp_sink) (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
  GstPad * (*request_rtp_src) (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
  GstPad * (*request_rtcp_sink) (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
  GstPad * (*request_rtcp_src) (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);

  gboolean (*custom_ssrc_management) (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, GstElement * ssrcdemux, guint ssrc, GstPad * pad);
};

GType kms_i_rtp_session_manager_get_type (void);

GstPad * kms_i_rtp_session_manager_request_rtp_sink (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
GstPad * kms_i_rtp_session_manager_request_rtp_src (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
GstPad * kms_i_rtp_session_manager_request_rtcp_sink (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
GstPad * kms_i_rtp_session_manager_request_rtcp_src (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, SdpMediaConfig *mconf);
gboolean kms_i_rtp_session_manager_custom_ssrc_management (KmsIRtpSessionManager *self, KmsBaseRtpSession *sess, GstElement * ssrcdemux, guint ssrc, GstPad * pad);

/* KmsIRtpSessionManager end */

G_END_DECLS
#endif /* __KMS_I_RTP_SESSION_MANAGER_H__ */
