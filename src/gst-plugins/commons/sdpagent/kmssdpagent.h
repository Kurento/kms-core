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
#ifndef __KMS_SDP_AGENT_H_
#define __KMS_SDP_AGENT_H_

#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

#include "kmssdpcontext.h"
#include "kmssdpmediahandler.h"
#include "kmssdpcontext.h"

G_BEGIN_DECLS

#define KMS_SDP_AGENT_ERROR \
  g_quark_from_static_string("kms-sdp-agent-error-quark")

typedef enum
{
  SDP_AGENT_INVALID_MEDIA,
  SDP_AGENT_INVALID_PARAMETER,
  SDP_AGENT_INVALID_PROTOCOL,
  SDP_AGENT_UNEXPECTED_ERROR
} SdpAgentError;

#define KMS_TYPE_SDP_AGENT \
  (kms_sdp_agent_get_type())

#define KMS_SDP_AGENT(obj) (            \
  G_TYPE_CHECK_INSTANCE_CAST (          \
    (obj),                              \
    KMS_TYPE_SDP_AGENT,                 \
    KmsSdpAgent                         \
  )                                     \
)
#define KMS_SDP_AGENT_CLASS(klass) (    \
  G_TYPE_CHECK_CLASS_CAST (             \
    (klass),                            \
    KMS_TYPE_SDP_AGENT,                 \
    KmsSdpAgentClass                    \
  )                                     \
)
#define KMS_IS_SDP_AGENT(obj) (         \
  G_TYPE_CHECK_INSTANCE_TYPE (          \
    (obj),                              \
    KMS_TYPE_SDP_AGENT                  \
  )                                     \
)
#define KMS_IS_SDP_AGENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SDP_AGENT))
#define KMS_SDP_AGENT_GET_CLASS(obj) (  \
  G_TYPE_INSTANCE_GET_CLASS (              \
    (obj),                                 \
    KMS_TYPE_SDP_AGENT,                 \
    KmsSdpAgentClass                    \
  )                                        \
)
typedef struct _KmsSdpAgent KmsSdpAgent;
typedef struct _KmsSdpAgentClass KmsSdpAgentClass;
typedef struct _KmsSdpAgentPrivate KmsSdpAgentPrivate;

struct _KmsSdpAgent
{
  GObject parent;

  /*< private > */
  KmsSdpAgentPrivate *priv;
};

struct _KmsSdpAgentClass
{
  GObjectClass parent_class;

  /* methods */
  gint (*add_proto_handler) (KmsSdpAgent * agent, const gchar *media, KmsSdpMediaHandler *handler);
  SdpMessageContext *(*create_offer) (KmsSdpAgent * agent, GError **error);
  SdpMessageContext *(*create_answer) (KmsSdpAgent * agent, const GstSDPMessage * offer, GError **error);
  void (*set_local_description) (KmsSdpAgent * agent, GstSDPMessage * description);
  void (*set_remote_description) (KmsSdpAgent * agent, GstSDPMessage * description);
  gint (*crate_bundle_group) (KmsSdpAgent * agent);
  gboolean (*add_handler_to_group) (KmsSdpAgent * agent, guint gid, guint mid);
};

GType kms_sdp_agent_get_type ();

KmsSdpAgent * kms_sdp_agent_new ();
gint kms_sdp_agent_add_proto_handler (KmsSdpAgent * agent, const gchar *media, KmsSdpMediaHandler *handler);
SdpMessageContext * kms_sdp_agent_create_offer (KmsSdpAgent * agent, GError **error);
SdpMessageContext * kms_sdp_agent_create_answer (KmsSdpAgent * agent, const GstSDPMessage * offer, GError **error);
void kms_sdp_agent_set_local_description (KmsSdpAgent * agent, GstSDPMessage * description);
void kms_sdp_agent_set_remote_description (KmsSdpAgent * agent, GstSDPMessage * description);
gint kms_sdp_agent_crate_bundle_group (KmsSdpAgent * agent);
gboolean kms_sdp_agent_add_handler_to_group (KmsSdpAgent * agent, guint gid, guint hid);

typedef gboolean (*KmsSdpAgentConfigureMediaCallback) (KmsSdpAgent *agent,
                                                   SdpMediaConfig *mconf,
                                                   gpointer user_data);
void kms_sdp_agent_set_configure_media_callback (KmsSdpAgent * agent,
                                             KmsSdpAgentConfigureMediaCallback callback,
                                             gpointer user_data,
                                             GDestroyNotify destroy);

G_END_DECLS

#endif /* __KMS_SDP_AGENT_H_ */
