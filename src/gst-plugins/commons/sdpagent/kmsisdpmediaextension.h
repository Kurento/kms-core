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
#ifndef _KMS_I_SDP_MEDIA_EXTENSION_H_
#define _KMS_I_SDP_MEDIA_EXTENSION_H_

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

#include "kmssdpcontext.h"

G_BEGIN_DECLS

#define KMS_TYPE_I_SDP_MEDIA_EXTENSION \
  (kms_i_sdp_media_extension_get_type())

#define KMS_I_SDP_MEDIA_EXTENSION(obj) ( \
  G_TYPE_CHECK_INSTANCE_CAST (           \
    (obj),                               \
    KMS_TYPE_I_SDP_MEDIA_EXTENSION,      \
    KmsISdpMediaExtension                \
  )                                      \
)
#define KMS_IS_I_SDP_MEDIA_EXTENSION(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (              \
    (obj),                                  \
    KMS_TYPE_I_SDP_MEDIA_EXTENSION          \
  )                                         \
)
#define KMS_I_SDP_MEDIA_EXTENSION_CAST(obj) ((KmsISdpMediaExtension*)(obj))
#define KMS_I_SDP_MEDIA_EXTENSION_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE(                    \
    (obj),                                           \
    KMS_TYPE_I_SDP_MEDIA_EXTENSION,                  \
    KmsISdpMediaExtensionInterface                   \
  )                                                  \
)

typedef struct _KmsISdpMediaExtension KmsISdpMediaExtension;
typedef struct _KmsISdpMediaExtensionInterface KmsISdpMediaExtensionInterface;

struct _KmsISdpMediaExtensionInterface
{
  GTypeInterface parent_iface;

  gboolean (*add_offer_attributes) (KmsISdpMediaExtension *ext, GstSDPMedia * offer, GError **error);
  gboolean (*add_answer_attributes) (KmsISdpMediaExtension *ext, const GstSDPMedia * offer, GstSDPMedia * answer, GError **error);
  gboolean (*can_insert_attribute) (KmsISdpMediaExtension *ext, const GstSDPMedia * offer, const GstSDPAttribute * attr, GstSDPMedia * answer, SdpMessageContext *ctx);
  gboolean (*process_answer_attributes) (KmsISdpMediaExtension *ext, const GstSDPMedia * answer, GError **error);
};

GType kms_i_sdp_media_extension_get_type (void);

gboolean kms_i_sdp_media_extension_add_offer_attributes (KmsISdpMediaExtension *ext, GstSDPMedia * offer, GError **error);
gboolean kms_i_sdp_media_extension_add_answer_attributes (KmsISdpMediaExtension *ext, const GstSDPMedia * offer, GstSDPMedia * answer, GError **error);
gboolean kms_i_sdp_media_extension_can_insert_attribute (KmsISdpMediaExtension *ext, const GstSDPMedia * offer, const GstSDPAttribute * attr, GstSDPMedia * answer, SdpMessageContext *ctx);
gboolean kms_i_sdp_media_extension_process_answer_attributes (KmsISdpMediaExtension *ext, const GstSDPMedia * answer, GError **error);

G_END_DECLS

#endif /* _KMS_SDP_MEDIA_HANDLER_H_ */
