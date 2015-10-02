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

#ifndef __KMS_SDP_BASE_GROUP_H__
#define __KMS_SDP_BASE_GROUP_H__

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

#include "kmsisdpsessionextension.h"
#include "kmssdpmediahandler.h"
#include "kmssdpagentcommon.h"

G_BEGIN_DECLS

#define KMS_SDP_BASE_GROUP_ERROR \
  g_quark_from_static_string("kms-sdp-base-group-error-quark")

typedef enum
{
  SDP_BASE_GROUP_HANDLER_NOT_FOUND,
  SDP_BASE_GROUP_UNEXPECTED_ERROR
} SdpBaseGroupError;

#define KMS_TYPE_SDP_BASE_GROUP \
  (kms_sdp_base_group_get_type())

#define KMS_SDP_BASE_GROUP(obj) (    \
  G_TYPE_CHECK_INSTANCE_CAST (       \
    (obj),                           \
    KMS_TYPE_SDP_BASE_GROUP,         \
    KmsSdpBaseGroup                  \
  )                                  \
)
#define KMS_SDP_BASE_GROUP_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                \
    (klass),                               \
    KMS_TYPE_SDP_BASE_GROUP,               \
    KmsSdpBaseGroupClass                   \
  )                                        \
)
#define KMS_IS_SDP_BASE_GROUP(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (       \
    (obj),                           \
    KMS_TYPE_SDP_BASE_GROUP          \
  )                                  \
)
#define KMS_IS_SDP_BASE_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_BASE_GROUP))
#define KMS_SDP_BASE_GROUP_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                \
    (obj),                                   \
    KMS_TYPE_SDP_BASE_GROUP,                 \
    KmsSdpBaseGroupClass                     \
  )                                          \
)

typedef struct _KmsSdpBaseGroup KmsSdpBaseGroup;
typedef struct _KmsSdpBaseGroupClass KmsSdpBaseGroupClass;
typedef struct _KmsSdpBaseGroupPrivate KmsSdpBaseGroupPrivate;

struct _KmsSdpBaseGroup
{
  GObject parent;

  /*< private > */
  KmsSdpBaseGroupPrivate *priv;
};

struct _KmsSdpBaseGroupClass
{
  GObjectClass parent_class;

  /*< public >*/
  gboolean (*add_media_handler) (KmsSdpBaseGroup *grp, KmsSdpHandler *handler, GError **error);
  gboolean (*remove_media_handler) (KmsSdpBaseGroup *grp, KmsSdpHandler *handler, GError **error);

  /*< protected>*/
  gboolean (*add_offer_attributes) (KmsSdpBaseGroup *grp, GstSDPMessage * offer, GError **error);
  gboolean (*add_answer_attributes) (KmsSdpBaseGroup *grp, const GstSDPMessage * offer, GstSDPMessage * answer, GError **error);
  gboolean (*can_insert_attribute) (KmsSdpBaseGroup *grp, const GstSDPMessage * offer, const GstSDPAttribute * attr, GstSDPMessage * answer);
};

GType kms_sdp_base_group_get_type ();

gboolean kms_sdp_base_group_add_media_handler (KmsSdpBaseGroup *grp, KmsSdpHandler *handler, GError **error);
gboolean kms_sdp_base_group_remove_media_handler (KmsSdpBaseGroup *grp, KmsSdpHandler *handler, GError **error);

G_END_DECLS

#endif /* __KMS_SDP_BASE_GROUP_H__ */
