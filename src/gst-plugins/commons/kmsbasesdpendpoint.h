/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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
#ifndef __KMS_BASE_SDP_ENDPOINT_H__
#define __KMS_BASE_SDP_ENDPOINT_H__

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>
#include "kmselement.h"
#include "sdpagent/kmssdpagent.h"
#include "sdpagent/kmssdpcontext.h"

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define KMS_TYPE_BASE_SDP_ENDPOINT \
  (kms_base_sdp_endpoint_get_type())
#define KMS_BASE_SDP_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_BASE_SDP_ENDPOINT,KmsBaseSdpEndpoint))
#define KMS_BASE_SDP_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_BASE_SDP_ENDPOINT,KmsBaseSdpEndpointClass))
#define KMS_IS_BASE_SDP_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_BASE_SDP_ENDPOINT))
#define KMS_IS_BASE_SDP_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_BASE_SDP_ENDPOINT))
#define KMS_BASE_SDP_ENDPOINT_CAST(obj) ((KmsBaseSdpEndpoint*)(obj))

typedef struct _KmsBaseSdpEndpointPrivate KmsBaseSdpEndpointPrivate;
typedef struct _KmsBaseSdpEndpoint KmsBaseSdpEndpoint;
typedef struct _KmsBaseSdpEndpointClass KmsBaseSdpEndpointClass;

#define KMS_BASE_SDP_ENDPOINT_LOCK(elem) \
  (g_rec_mutex_lock (&KMS_BASE_SDP_ENDPOINT_CAST ((elem))->media_mutex))
#define KMS_BASE_SDP_ENDPOINT_UNLOCK(elem) \
  (g_rec_mutex_unlock (&KMS_BASE_SDP_ENDPOINT_CAST ((elem))->media_mutex))

struct _KmsBaseSdpEndpoint
{
  KmsElement parent;

  KmsBaseSdpEndpointPrivate *priv;
};

struct _KmsBaseSdpEndpointClass
{
  KmsElementClass parent_class;

  /* private */
  /* actions */
  GstSDPMessage *(*generate_offer) (KmsBaseSdpEndpoint * self);
  GstSDPMessage *(*process_offer) (KmsBaseSdpEndpoint * self, GstSDPMessage * offer);
  void (*process_answer) (KmsBaseSdpEndpoint * self, GstSDPMessage * answer);

  /* virtual methods */
  void (*start_transport_send) (KmsBaseSdpEndpoint * self, SdpMessageContext * remote_ctx);
  void (*connect_input_elements) (KmsBaseSdpEndpoint * self, SdpMessageContext * negotiated_ctx);

  gboolean (*configure_media) (KmsBaseSdpEndpoint * self, SdpMediaConfig * mconf);

  /* Virtual handler factory methods */
  void (*create_media_handler) (KmsBaseSdpEndpoint * self, KmsSdpMediaHandler **handler);
};

GType kms_base_sdp_endpoint_get_type (void);

SdpMessageContext *kms_base_sdp_endpoint_get_local_sdp_ctx (KmsBaseSdpEndpoint * self);
GstSDPMessage *kms_base_sdp_endpoint_get_local_sdp (KmsBaseSdpEndpoint * self);
void kms_base_sdp_endpoint_set_local_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * local_sdp);

SdpMessageContext *kms_base_sdp_endpoint_get_remote_sdp_ctx (KmsBaseSdpEndpoint * self);
GstSDPMessage *kms_base_sdp_endpoint_get_remote_sdp (KmsBaseSdpEndpoint * self);
void kms_base_sdp_endpoint_set_remote_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * remote_sdp);

SdpMessageContext *kms_base_sdp_endpoint_get_negotiated_sdp_ctx (KmsBaseSdpEndpoint * self);

void kms_base_sdp_endpoint_create_media_handler (KmsBaseSdpEndpoint * self, KmsSdpMediaHandler **handler);

G_END_DECLS
#endif /* __KMS_BASE_SDP_ENDPOINT_H__ */
