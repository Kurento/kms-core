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

#ifndef __KMS_SDP_REJECT_MEDIA_HANDLER_H__
#define __KMS_SDP_REJECT_MEDIA_HANDLER_H__

#include "kmssdpmediahandler.h"

G_BEGIN_DECLS

#define KMS_TYPE_SDP_REJECT_MEDIA_HANDLER \
  (kms_sdp_reject_media_handler_get_type())

#define KMS_SDP_REJECT_MEDIA_HANDLER(obj) (  \
  G_TYPE_CHECK_INSTANCE_CAST (               \
    (obj),                                   \
    KMS_TYPE_SDP_REJECT_MEDIA_HANDLER,       \
    KmsSdpRejectMediaHandler                 \
  )                                          \
)
#define KMS_SDP_REJECT_MEDIA_HANDLER_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (                         \
    (klass),                                        \
    KMS_TYPE_SDP_REJECT_MEDIA_HANDLER,              \
    KmsSdpRejectMediaHandlerClass                   \
  )                                                 \
)
#define KMS_IS_SDP_REJECT_MEDIA_HANDLER(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (                 \
    (obj),                                     \
    KMS_TYPE_SDP_REJECT_MEDIA_HANDLER          \
  )                                            \
)
#define KMS_IS_SDP_REJECT_MEDIA_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_REJECT_MEDIA_HANDLER))
#define KMS_SDP_REJECT_MEDIA_HANDLER_GET_CLASS(obj) ( \
  G_TYPE_INSTANCE_GET_CLASS (                         \
    (obj),                                            \
    KMS_TYPE_SDP_REJECT_MEDIA_HANDLER,                \
    KmsSdpRejectMediaHandlerClass                     \
  )                                                   \
)

typedef struct _KmsSdpRejectMediaHandler KmsSdpRejectMediaHandler;
typedef struct _KmsSdpRejectMediaHandlerClass KmsSdpRejectMediaHandlerClass;
typedef struct _KmsSdpRejectMediaHandlerPrivate KmsSdpRejectMediaHandlerPrivate;

struct _KmsSdpRejectMediaHandler
{
  KmsSdpMediaHandler parent;

  /*< private > */
  KmsSdpRejectMediaHandlerPrivate *priv;
};

struct _KmsSdpRejectMediaHandlerClass
{
  KmsSdpMediaHandlerClass parent_class;
};

GType kms_sdp_reject_media_handler_get_type ();

KmsSdpRejectMediaHandler * kms_sdp_reject_media_handler_new ();

G_END_DECLS

#endif /* __KMS_SDP_REJECT_MEDIA_HANDLER_H__ */
