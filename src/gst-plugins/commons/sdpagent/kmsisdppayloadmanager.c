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

#include "kmsisdppayloadmanager.h"

G_DEFINE_INTERFACE (KmsISdpPayloadManager, kms_i_sdp_payload_manager, 0);

static void
kms_i_sdp_payload_manager_default_init (KmsISdpPayloadManagerInterface * iface)
{
  /* Nothing to do */
}

gint
kms_i_sdp_payload_manager_get_dynamic_pt (KmsISdpPayloadManager * self,
    GError ** error)
{
  g_return_val_if_fail (KMS_IS_I_SDP_PAYLOAD_MANAGER (self), -1);

  return KMS_I_SDP_PAYLOAD_MANAGER_GET_INTERFACE (self)->get_dynamic_pt (self,
      error);
}
