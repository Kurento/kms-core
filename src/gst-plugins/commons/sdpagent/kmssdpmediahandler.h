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
#ifndef _KMS_SDP_MEDIA_HANDLER_H_
#define _KMS_SDP_MEDIA_HANDLER_H_

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

#include "kmssdpcontext.h"
#include "kmsisdpmediaextension.h"

G_BEGIN_DECLS

#define KMS_TYPE_SDP_MEDIA_HANDLER \
  (kms_sdp_media_handler_get_type())

#define KMS_SDP_MEDIA_HANDLER(obj) (    \
  G_TYPE_CHECK_INSTANCE_CAST (          \
    (obj),                              \
    KMS_TYPE_SDP_MEDIA_HANDLER,         \
    KmsSdpMediaHandler                  \
  )                                     \
)
#define KMS_SDP_MEDIA_HANDLER_CLASS(klass) (  \
  G_TYPE_CHECK_CLASS_CAST (                   \
    (klass),                                  \
    KMS_TYPE_SDP_MEDIA_HANDLER,               \
    KmsSdpMediaHandlerClass                   \
  )                                           \
)
#define KMS_IS_SDP_MEDIA_HANDLER(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (          \
    (obj),                              \
    KMS_TYPE_SDP_MEDIA_HANDLER          \
  )                                     \
)
#define KMS_IS_SDP_MEDIA_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_MEDIA_HANDLER))
#define KMS_SDP_MEDIA_HANDLER_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (                   \
    (obj),                                      \
    KMS_TYPE_SDP_MEDIA_HANDLER,                 \
    KmsSdpMediaHandlerClass                     \
  )                                             \
)

typedef struct _KmsSdpMediaHandler KmsSdpMediaHandler;
typedef struct _KmsSdpMediaHandlerClass KmsSdpMediaHandlerClass;
typedef struct _KmsSdpMediaHandlerPrivate KmsSdpMediaHandlerPrivate;

struct _KmsSdpMediaHandler
{
  GObject parent;

  /*< private > */
  KmsSdpMediaHandlerPrivate *priv;
};

struct _KmsSdpMediaHandlerClass
{
  GObjectClass parent_class;

  /* public methods */
  GstSDPMedia * (*create_offer) (KmsSdpMediaHandler *handler, const gchar *media, GError **error);
  GstSDPMedia * (*create_answer) (KmsSdpMediaHandler *handler, SdpMessageContext *ctx, const GstSDPMedia * offer, GError **error);
  gboolean (*process_answer) (KmsSdpMediaHandler *handler, const GstSDPMedia * answer, GError **error);

  void (*add_bandwidth) (KmsSdpMediaHandler *handler, const gchar *bwtype, guint bandwidth);
  gboolean (*manage_protocol) (KmsSdpMediaHandler *handler, const gchar *protocol);
  gboolean (*add_media_extension) (KmsSdpMediaHandler *handler, KmsISdpMediaExtension *ext);

  /* private methods */
  gboolean (*can_insert_attribute) (KmsSdpMediaHandler *handler, const GstSDPMedia * offer, const GstSDPAttribute * attr, GstSDPMedia * answer, SdpMessageContext *ctx);
  gboolean (*intersect_sdp_medias) (KmsSdpMediaHandler *handler, const GstSDPMedia * offer, GstSDPMedia * answer, SdpMessageContext *ctx, GError **error);

  gboolean (*init_offer) (KmsSdpMediaHandler *handler, const gchar * media, GstSDPMedia * offer, GError **error);
  gboolean (*add_offer_attributes) (KmsSdpMediaHandler *handler, GstSDPMedia * offer, GError **error);

  gboolean (*init_answer) (KmsSdpMediaHandler *handler, const GstSDPMedia * offer, GstSDPMedia * answer, GError **error);
  gboolean (*add_answer_attributes) (KmsSdpMediaHandler *handler, const GstSDPMedia * offer, GstSDPMedia * answer, GError **error);
};

GType kms_sdp_media_handler_get_type ();

GstSDPMedia * kms_sdp_media_handler_create_offer (KmsSdpMediaHandler *handler, const gchar *media, GError **error);
GstSDPMedia * kms_sdp_media_handler_create_answer (KmsSdpMediaHandler *handler, SdpMessageContext *ctx, const GstSDPMedia * offer, GError **error);
gboolean kms_sdp_media_handler_process_answer (KmsSdpMediaHandler *handler, const GstSDPMedia * answer, GError **error);
void kms_sdp_media_handler_add_bandwidth (KmsSdpMediaHandler *handler, const gchar *bwtype, guint bandwidth);
gboolean kms_sdp_media_handler_manage_protocol (KmsSdpMediaHandler *handler, const gchar *protocol);
gboolean kms_sdp_media_handler_add_media_extension (KmsSdpMediaHandler *handler, KmsISdpMediaExtension *ext);

G_END_DECLS

#endif /* _KMS_SDP_MEDIA_HANDLER_H_ */
