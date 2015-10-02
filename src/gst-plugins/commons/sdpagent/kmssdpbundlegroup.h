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

#ifndef __KMS_SDP_BUNDLE_GROUP_H__
#define __KMS_SDP_BUNDLE_GROUP_H__

#include "kmssdpbasegroup.h"

G_BEGIN_DECLS

#define KMS_TYPE_SDP_BUNDLE_GROUP \
  (kms_sdp_bundle_group_get_type())

#define KMS_SDP_BUNDLE_GROUP(obj) (    \
  G_TYPE_CHECK_INSTANCE_CAST (         \
    (obj),                             \
    KMS_TYPE_SDP_BUNDLE_GROUP,         \
    KmsSdpBundleGroup                  \
  )                                    \
)
#define KMS_SDP_BUNDLE_GROUP_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                  \
    (klass),                                 \
    KMS_TYPE_SDP_BUNDLE_GROUP,               \
    KmsSdpBundleGroupClass                   \
  )                                          \
)
#define KMS_IS_SDP_BUNDLE_GROUP(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (         \
    (obj),                             \
    KMS_TYPE_SDP_BUNDLE_GROUP          \
  )                                    \
)
#define KMS_IS_SDP_BUNDLE_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_BUNDLE_GROUP))
#define KMS_SDP_BUNDLE_GROUP_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                  \
    (obj),                                     \
    KMS_TYPE_SDP_BUNDLE_GROUP,                 \
    KmsSdpBundleGroupClass                     \
  )                                            \
)

typedef struct _KmsSdpBundleGroup KmsSdpBundleGroup;
typedef struct _KmsSdpBundleGroupClass KmsSdpBundleGroupClass;
typedef struct _KmsSdpBundleGroupPrivate KmsSdpBundleGroupPrivate;

struct _KmsSdpBundleGroup
{
  KmsSdpBaseGroup parent;

  /*< private > */
  KmsSdpBundleGroupPrivate *priv;
};

struct _KmsSdpBundleGroupClass
{
  KmsSdpBaseGroupClass parent_class;
};

GType kms_sdp_bundle_group_get_type ();

KmsSdpBundleGroup * kms_sdp_bundle_group_new ();

G_END_DECLS

#endif /* __KMS_SDP_BUNDLE_GROUP_H__ */
