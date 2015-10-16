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

#include "kmsisdpmediaextension.h"

G_DEFINE_INTERFACE (KmsISdpMediaExtension, kms_i_sdp_media_extension, 0);

static void
kms_i_sdp_media_extension_default_init (KmsISdpMediaExtensionInterface * iface)
{
  /* Nothing to do */
}

gboolean
kms_i_sdp_media_extension_add_offer_attributes (KmsISdpMediaExtension * ext,
    GstSDPMedia * offer, GError ** error)
{
  g_return_if_fail (KMS_IS_I_SDP_MEDIA_EXTENSION (ext));

  return
      KMS_I_SDP_MEDIA_EXTENSION_GET_INTERFACE (ext)->add_offer_attributes (ext,
      offer, error);
}

gboolean
kms_i_sdp_media_extension_add_answer_attributes (KmsISdpMediaExtension * ext,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  g_return_if_fail (KMS_IS_I_SDP_MEDIA_EXTENSION (ext));

  return
      KMS_I_SDP_MEDIA_EXTENSION_GET_INTERFACE (ext)->add_answer_attributes (ext,
      offer, answer, error);
}

gboolean
kms_i_sdp_media_extension_can_insert_attribute (KmsISdpMediaExtension * ext,
    const GstSDPMedia * offer, const GstSDPAttribute * attr,
    GstSDPMedia * answer, SdpMessageContext * ctx)
{
  g_return_if_fail (KMS_IS_I_SDP_MEDIA_EXTENSION (ext));

  return
      KMS_I_SDP_MEDIA_EXTENSION_GET_INTERFACE (ext)->can_insert_attribute (ext,
      offer, attr, answer, ctx);
}

gboolean
kms_i_sdp_media_extension_process_answer_attributes (KmsISdpMediaExtension *
    ext, const GstSDPMedia * answer, GError ** error)
{
  g_return_if_fail (KMS_IS_I_SDP_MEDIA_EXTENSION (ext));

  return
      KMS_I_SDP_MEDIA_EXTENSION_GET_INTERFACE (ext)->process_answer_attributes
      (ext, answer, error);
}
