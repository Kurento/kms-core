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

#ifndef __KMS_I_SDP_PAYLOAD_MANAGER_H__
#define __KMS_I_SDP_PAYLOAD_MANAGER_H__

#include <gst/gst.h>

#define KMS_TYPE_I_SDP_PAYLOAD_MANAGER \
  (kms_i_sdp_payload_manager_get_type ())
#define KMS_I_SDP_PAYLOAD_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), KMS_TYPE_I_SDP_PAYLOAD_MANAGER, KmsISdpPayloadManager))
#define KMS_IS_I_SDP_PAYLOAD_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KMS_TYPE_I_SDP_PAYLOAD_MANAGER))
#define KMS_I_SDP_PAYLOAD_MANAGER_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), KMS_TYPE_I_SDP_PAYLOAD_MANAGER, KmsISdpPayloadManagerInterface))

typedef struct _KmsISdpPayloadManager               KmsISdpPayloadManager;
typedef struct _KmsISdpPayloadManagerInterface      KmsISdpPayloadManagerInterface;

struct _KmsISdpPayloadManagerInterface
{
  GTypeInterface parent;

  gint (*get_dynamic_pt) (KmsISdpPayloadManager *self, GError **error);
};

GType kms_i_sdp_payload_manager_get_type (void);
gint kms_i_sdp_payload_manager_get_dynamic_pt (KmsISdpPayloadManager *self, GError **error);

#endif /* __KMS_I_SDP_PAYLOAD_MANAGER_H__ */
