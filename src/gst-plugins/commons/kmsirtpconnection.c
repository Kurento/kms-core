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

#include "kmsirtpconnection.h"

/* KmsIRtpConnection begin */
G_DEFINE_INTERFACE (KmsIRtpConnection, kms_i_rtp_connection, 0);

static void
kms_i_rtp_connection_default_init (KmsIRtpConnectionInterface * iface)
{
  /* Nothing to do */
}

void
kms_i_rtp_connection_add (KmsIRtpConnection * self, GstBin * bin,
    gboolean local_offer)
{
  g_return_if_fail (KMS_IS_I_RTP_CONNECTION (self));

  KMS_I_RTP_CONNECTION_GET_INTERFACE (self)->add (self, bin, local_offer);
}

GstPad *
kms_i_rtp_connection_request_rtp_sink (KmsIRtpConnection * self)
{
  g_return_val_if_fail (KMS_IS_I_RTP_CONNECTION (self), NULL);

  return KMS_I_RTP_CONNECTION_GET_INTERFACE (self)->request_rtp_sink (self);
}

GstPad *
kms_i_rtp_connection_request_rtp_src (KmsIRtpConnection * self)
{
  g_return_val_if_fail (KMS_IS_I_RTP_CONNECTION (self), NULL);

  return KMS_I_RTP_CONNECTION_GET_INTERFACE (self)->request_rtp_src (self);
}

GstPad *
kms_i_rtp_connection_request_rtcp_sink (KmsIRtpConnection * self)
{
  g_return_val_if_fail (KMS_IS_I_RTP_CONNECTION (self), NULL);

  return KMS_I_RTP_CONNECTION_GET_INTERFACE (self)->request_rtcp_sink (self);
}

GstPad *
kms_i_rtp_connection_request_rtcp_src (KmsIRtpConnection * self)
{
  g_return_val_if_fail (KMS_IS_I_RTP_CONNECTION (self), NULL);

  return KMS_I_RTP_CONNECTION_GET_INTERFACE (self)->request_rtcp_src (self);
}

/* KmsIRtpConnection end */

/* KmsIRtcpMuxConnection begin */
G_DEFINE_INTERFACE (KmsIRtcpMuxConnection, kms_i_rtcp_mux_connection,
    KMS_TYPE_I_RTP_CONNECTION);

static void
kms_i_rtcp_mux_connection_default_init (KmsIRtcpMuxConnectionInterface * iface)
{
  /* Nothing to do */
}

/* KmsIRtcpMuxConnection end */

/* KmsIBundleConnection begin */
G_DEFINE_INTERFACE (KmsIBundleConnection, kms_i_bundle_connection,
    KMS_TYPE_I_RTCP_MUX_CONNECTION);

static void
kms_i_bundle_connection_default_init (KmsIBundleConnectionInterface * iface)
{
  /* Nothing to do */
}

/* KmsIBundleConnection end */
