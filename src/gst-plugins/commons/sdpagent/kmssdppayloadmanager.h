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
#ifndef _KMS_SDP_PAYLOAD_MANAGER_H_
#define _KMS_SDP_PAYLOAD_MANAGER_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_SDP_PAYLOAD_MANAGER \
  (kms_sdp_payload_manager_get_type())

#define KMS_SDP_PAYLOAD_MANAGER(obj) (  \
  G_TYPE_CHECK_INSTANCE_CAST (          \
    (obj),                              \
    KMS_TYPE_SDP_PAYLOAD_MANAGER,       \
    KmsSdpPayloadManager                \
  )                                     \
)
#define KMS_SDP_PAYLOAD_MANAGER_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                     \
    (klass),                                    \
    KMS_TYPE_SDP_PAYLOAD_MANAGER,               \
    KmsSdpPayloadManagerClass                   \
  )                                             \
)
#define KMS_IS_SDP_PAYLOAD_MANAGER(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (            \
    (obj),                                \
    KMS_TYPE_SDP_PAYLOAD_MANAGER          \
  )                                       \
)
#define KMS_IS_SDP_PAYLOAD_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_PAYLOAD_MANAGER))
#define KMS_SDP_PAYLOAD_MANAGER_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                     \
    (obj),                                        \
    KMS_TYPE_SDP_PAYLOAD_MANAGER,                 \
    KmsSdpPayloadManagerClass                     \
  )                                               \
)

typedef struct _KmsSdpPayloadManager KmsSdpPayloadManager;
typedef struct _KmsSdpPayloadManagerClass KmsSdpPayloadManagerClass;
typedef struct _KmsSdpPayloadManagerPrivate KmsSdpPayloadManagerPrivate;

struct _KmsSdpPayloadManager
{
  GObject parent;

  /*< private > */
  KmsSdpPayloadManagerPrivate *priv;
};

struct _KmsSdpPayloadManagerClass
{
  GObjectClass parent_class;
};

GType kms_sdp_payload_manager_get_type ();

KmsSdpPayloadManager * kms_sdp_payload_manager_new ();

G_END_DECLS

#endif /* _KMS_SDP_PAYLOAD_MANAGER_H_ */
