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

#include "kmsirtpsessionmanager.h"

G_DEFINE_INTERFACE (KmsIRtpSessionManager, kms_i_rtp_session_manager, 0);

static void
kms_i_rtp_session_manager_default_init (KmsIRtpSessionManagerInterface * iface)
{
  /* do nothing */
}

GstPad *
kms_i_rtp_session_manager_request_rtp_sink (KmsIRtpSessionManager * self,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  g_return_val_if_fail (KMS_IS_I_RTP_SESSION_MANAGER (self), NULL);

  return KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE (self)->request_rtp_sink (self,
      sess, mconf);
}

GstPad *
kms_i_rtp_session_manager_request_rtp_src (KmsIRtpSessionManager * self,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  g_return_val_if_fail (KMS_IS_I_RTP_SESSION_MANAGER (self), NULL);

  return KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE (self)->request_rtp_src (self,
      sess, mconf);
}

GstPad *
kms_i_rtp_session_manager_request_rtcp_sink (KmsIRtpSessionManager * self,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  g_return_val_if_fail (KMS_IS_I_RTP_SESSION_MANAGER (self), NULL);

  return
      KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE (self)->request_rtcp_sink (self,
      sess, mconf);
}

GstPad *
kms_i_rtp_session_manager_request_rtcp_src (KmsIRtpSessionManager * self,
    KmsBaseRtpSession * sess, SdpMediaConfig * mconf)
{
  g_return_val_if_fail (KMS_IS_I_RTP_SESSION_MANAGER (self), NULL);

  return KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE (self)->request_rtcp_src (self,
      sess, mconf);
}

gboolean
kms_i_rtp_session_manager_custom_ssrc_management (KmsIRtpSessionManager * self,
    KmsBaseRtpSession * sess, GstElement * ssrcdemux, guint ssrc, GstPad * pad)
{
  g_return_val_if_fail (KMS_IS_I_RTP_SESSION_MANAGER (self), NULL);

  return
      KMS_I_RTP_SESSION_MANAGER_GET_INTERFACE (self)->custom_ssrc_management
      (self, sess, ssrcdemux, ssrc, pad);
}
