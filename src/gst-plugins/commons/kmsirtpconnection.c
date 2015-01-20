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

enum
{
  SIGNAL_CONNECTED,
  LAST_SIGNAL
};

static guint kms_i_rtp_connection_signals[LAST_SIGNAL] = { 0 };

/* KmsIRtpConnection begin */
G_DEFINE_INTERFACE (KmsIRtpConnection, kms_i_rtp_connection, 0);

static void
kms_i_rtp_connection_default_init (KmsIRtpConnectionInterface * iface)
{
  kms_i_rtp_connection_signals[SIGNAL_CONNECTED] =
      g_signal_new ("connected",
      G_TYPE_FROM_CLASS (iface),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsIRtpConnectionInterface, connected_signal), NULL,
      NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
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

void
kms_i_rtp_connection_connected_signal (KmsIRtpConnection * self)
{
  g_signal_emit (G_OBJECT (self),
      kms_i_rtp_connection_signals[SIGNAL_CONNECTED], 0);
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
