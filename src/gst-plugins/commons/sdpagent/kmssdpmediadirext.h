/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
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

#ifndef __KMS_SDP_MEDIA_DIRECTION_EXT_H__
#define __KMS_SDP_MEDIA_DIRECTION_EXT_H__

#include <gst/gst.h>

#include "sdp_utils.h"

G_BEGIN_DECLS

#define KMS_TYPE_SDP_MEDIA_DIRECTION_EXT    \
  (kms_sdp_media_direction_ext_get_type())

#define KMS_SDP_MEDIA_DIRECTION_EXT(obj) (  \
  G_TYPE_CHECK_INSTANCE_CAST (              \
    (obj),                                  \
    KMS_TYPE_SDP_MEDIA_DIRECTION_EXT,       \
    KmsSdpMediaDirectionExt                 \
  )                                         \
)
#define KMS_SDP_MEDIA_DIRECTION_EXT_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                         \
    (klass),                                        \
    KMS_TYPE_SDP_MEDIA_DIRECTION_EXT,               \
    KmsSdpMediaDirectionExtClass                    \
  )                                                 \
)
#define KMS_IS_SDP_MEDIA_DIRECTION_EXT(obj) (  \
  G_TYPE_CHECK_INSTANCE_TYPE (                 \
    (obj),                                     \
    KMS_TYPE_SDP_MEDIA_DIRECTION_EXT           \
  )                                            \
)
#define KMS_IS_SDP_MEDIA_DIRECTION_EXT_CLASS(klass)  \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_MEDIA_DIRECTION_EXT))
#define KMS_SDP_MEDIA_DIRECTION_EXT_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                         \
    (obj),                                            \
    KMS_TYPE_SDP_MEDIA_DIRECTION_EXT,                 \
    KmsSdpMediaDirectionExtClass                      \
  )                                                   \
)

typedef struct _KmsSdpMediaDirectionExt KmsSdpMediaDirectionExt;
typedef struct _KmsSdpMediaDirectionExtClass KmsSdpMediaDirectionExtClass;
typedef struct _KmsSdpMediaDirectionExtPrivate KmsSdpMediaDirectionExtPrivate;

struct _KmsSdpMediaDirectionExt
{
  GObject parent;

  /*< private > */
  KmsSdpMediaDirectionExtPrivate *priv;
};

struct _KmsSdpMediaDirectionExtClass
{
  GObjectClass parent_class;

  /* signals */
  GstSDPDirection (*on_offer_media_direction) (KmsSdpMediaDirectionExt * ext);
  GstSDPDirection (*on_answer_media_direction) (KmsSdpMediaDirectionExt * ext, GstSDPDirection dir);
  void (*on_answered_media_direction) (KmsSdpMediaDirectionExt * ext, GstSDPDirection dir);
};

GType kms_sdp_media_direction_ext_get_type ();

KmsSdpMediaDirectionExt * kms_sdp_media_direction_ext_new ();

G_END_DECLS

#endif /* __KMS_SDP_MEDIA_DIRECTION_EXT_H__ */
