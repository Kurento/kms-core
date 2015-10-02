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

#ifndef __KMS_SDP_MID_EXT_H__
#define __KMS_SDP_MID_EXT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_SDP_MID_EXT    \
  (kms_sdp_mid_ext_get_type())

#define KMS_SDP_MID_EXT(obj) (  \
  G_TYPE_CHECK_INSTANCE_CAST (  \
    (obj),                      \
    KMS_TYPE_SDP_MID_EXT,       \
    KmsSdpMidExt                \
  )                             \
)
#define KMS_SDP_MID_EXT_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (             \
    (klass),                            \
    KMS_TYPE_SDP_MID_EXT,               \
    KmsSdpMidExtClass                   \
  )                                     \
)
#define KMS_IS_SDP_MID_EXT(obj) (  \
  G_TYPE_CHECK_INSTANCE_TYPE (     \
    (obj),                         \
    KMS_TYPE_SDP_MID_EXT           \
  )                                \
)
#define KMS_IS_SDP_MID_EXT_CLASS(klass)  \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_MID_EXT))
#define KMS_SDP_MID_EXT_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (             \
    (obj),                                \
    KMS_TYPE_SDP_MID_EXT,                 \
    KmsSdpMidExtClass                     \
  )                                       \
)

typedef struct _KmsSdpMidExt KmsSdpMidExt;
typedef struct _KmsSdpMidExtClass KmsSdpMidExtClass;
typedef struct _KmsSdpMidExtPrivate KmsSdpMidExtPrivate;

struct _KmsSdpMidExt
{
  GObject parent;

  /*< private > */
  KmsSdpMidExtPrivate *priv;
};

struct _KmsSdpMidExtClass
{
  GObjectClass parent_class;

  /* signals */
  gchar * (*on_offer_mid) (KmsSdpMidExt * ext);
  gboolean (*on_answer_mid) (KmsSdpMidExt * ext, gchar *mid);
};

GType kms_sdp_mid_ext_get_type ();

KmsSdpMidExt * kms_sdp_mid_ext_new ();

G_END_DECLS

#endif /* __KMS_SDP_MID_EXT_H__ */
