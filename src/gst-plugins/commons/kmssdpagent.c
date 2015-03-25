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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "kmssdpagent.h"
#include "kms-core-marshal.h"

#define PLUGIN_NAME "sdpagent"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_agent_debug_category);
#define GST_CAT_DEFAULT kms_sdp_agent_debug_category

#define parent_class kms_sdp_agent_parent_class

#define USE_IPV6_DEFAULT FALSE
#define BUNDLE_DEFAULT FALSE

#define ORIGIN_ATTR_NETTYPE "IN"
#define ORIGIN_ATTR_ADDR_TYPE_IP4 "IP4"
#define ORIGIN_ATTR_ADDR_TYPE_IP6 "IP6"
#define DEFAULT_IP4_ADDR "0.0.0.0"
#define DEFAULT_IP6_ADDR "::"

/* Object properties */
enum
{
  PROP_0,
  PROP_BUNDLE,
  PROP_USE_IPV6,
  PROP_LOCAL_DESC,
  PROP_REMOTE_DESC,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

#define KMS_SDP_AGENT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (           \
    (obj),                                \
    KMS_TYPE_SDP_AGENT,                   \
    KmsSdpAgentPrivate                    \
  )                                       \
)
struct _KmsSdpAgentPrivate
{
  GstSDPMessage *local_description;
  GstSDPMessage *remote_description;
  gboolean use_ipv6;
  gboolean bundle;

  GHashTable *medias;

  GMutex mutex;
};

#define SDP_AGENT_LOCK(agent) \
  (g_mutex_lock (&KMS_SDP_AGENT ((agent))->priv->mutex))
#define SDP_AGENT_UNLOCK(agent) \
  (g_mutex_unlock (&KMS_SDP_AGENT ((agent))->priv->mutex))

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsSdpAgent, kms_sdp_agent,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_agent_debug_category, PLUGIN_NAME,
        0, "debug category for sdp agent"));

static void
kms_sdp_agent_release_sdp (GstSDPMessage ** sdp)
{
  if (*sdp == NULL) {
    return;
  }

  gst_sdp_message_free (*sdp);
  *sdp = NULL;
}

static void
kms_sdp_agent_finalize (GObject * object)
{
  KmsSdpAgent *self = KMS_SDP_AGENT (object);

  GST_DEBUG_OBJECT (self, "Finalize");

  kms_sdp_agent_release_sdp (&self->priv->local_description);
  kms_sdp_agent_release_sdp (&self->priv->remote_description);
  g_hash_table_unref (self->priv->medias);

  g_mutex_clear (&self->priv->mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_sdp_agent_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsSdpAgent *self = KMS_SDP_AGENT (object);

  SDP_AGENT_LOCK (self);

  switch (prop_id) {
    case PROP_BUNDLE:
      g_value_set_boolean (value, self->priv->bundle);
      break;
    case PROP_LOCAL_DESC:
      g_value_set_boxed (value, self->priv->local_description);
      break;
    case PROP_REMOTE_DESC:
      g_value_set_boxed (value, self->priv->remote_description);
      break;
    case PROP_USE_IPV6:
      g_value_set_boolean (value, self->priv->use_ipv6);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  SDP_AGENT_UNLOCK (self);
}

static void
kms_sdp_agent_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsSdpAgent *self = KMS_SDP_AGENT (object);

  SDP_AGENT_LOCK (self);

  switch (prop_id) {
    case PROP_BUNDLE:
      self->priv->bundle = g_value_get_boolean (value);
      break;
    case PROP_USE_IPV6:
      self->priv->use_ipv6 = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  SDP_AGENT_UNLOCK (self);
}

static gboolean
kms_sdp_agent_set_default_session_attributes (KmsSdpAgent * agent,
    GstSDPMessage * offer, GError ** error)
{
  const gchar *addrtype;
  const gchar *addr;
  const gchar *err_attr;

  SDP_AGENT_LOCK (agent);

  addrtype =
      (agent->priv->
      use_ipv6) ? ORIGIN_ATTR_ADDR_TYPE_IP6 : ORIGIN_ATTR_ADDR_TYPE_IP4;
  addr = (agent->priv->use_ipv6) ? DEFAULT_IP6_ADDR : DEFAULT_IP4_ADDR;

  SDP_AGENT_UNLOCK (agent);

  if (gst_sdp_message_set_version (offer, "0") != GST_SDP_OK) {
    err_attr = "version";
    goto error;
  }

  if (gst_sdp_message_set_origin (offer, "-", "0", "0", ORIGIN_ATTR_NETTYPE,
          addrtype, addr) != GST_SDP_OK) {
    err_attr = "origin";
    goto error;
  }

  if (gst_sdp_message_set_session_name (offer,
          "Kurento Media Server") != GST_SDP_OK) {
    err_attr = "session";
    goto error;
  }

  if (gst_sdp_message_set_connection (offer, ORIGIN_ATTR_NETTYPE, addrtype,
          addr, 0, 0) != GST_SDP_OK) {
    err_attr = "connection";
    goto error;
  }

  return TRUE;

error:
  g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
      "Can not set attr: %s", err_attr);

  return FALSE;
}

static gboolean
kms_sdp_agent_add_proto_handler_impl (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  GHashTable *handlers;
  gboolean ret = FALSE;
  gchar *proto;

  g_object_get (handler, "proto", &proto, NULL);

  if (proto == NULL) {
    GST_WARNING_OBJECT (agent, "Handler's proto can't be NULL");
    return FALSE;
  }

  SDP_AGENT_LOCK (agent);

  handlers = g_hash_table_lookup (agent->priv->medias, media);

  if (handlers == NULL) {
    /* create handlers map for this media */
    handlers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        (GDestroyNotify) g_object_unref);
    g_hash_table_insert (agent->priv->medias, g_strdup (media), handlers);
  }

  ret = g_hash_table_insert (handlers, proto, handler);

  SDP_AGENT_UNLOCK (agent);

  return ret;
}

static GstSDPMessage *
kms_sdp_agent_create_offer_impl (KmsSdpAgent * agent, GError ** error)
{
  GstSDPMessage *offer = NULL;

  gst_sdp_message_new (&offer);
  if (!kms_sdp_agent_set_default_session_attributes (agent, offer, error)) {
    kms_sdp_agent_release_sdp (&offer);
  }

  /* TODO: Add medias */

  return offer;
}

static GstSDPMessage *
kms_sdp_agent_create_answer_impl (KmsSdpAgent * agent,
    const GstSDPMessage * offer)
{
  /* TODO: */

  return NULL;
}

static void
kms_sdp_agent_set_local_description_impl (KmsSdpAgent * agent,
    GstSDPMessage * description)
{
  /* TODO: */
}

static void
kms_sdp_agent_set_remote_description_impl (KmsSdpAgent * agent,
    GstSDPMessage * description)
{
  /* TODO: */
}

static void
kms_sdp_agent_class_init (KmsSdpAgentClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = kms_sdp_agent_get_property;
  gobject_class->set_property = kms_sdp_agent_set_property;
  gobject_class->finalize = kms_sdp_agent_finalize;

  obj_properties[PROP_BUNDLE] = g_param_spec_boolean ("bundle",
      "Use BUNDLE group in offers",
      "Bundle media in offers when possible",
      BUNDLE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_USE_IPV6] = g_param_spec_boolean ("use-ipv6",
      "Use ipv6 in SDPs",
      "Use ipv6 addresses in generated sdp offers and answers",
      USE_IPV6_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_LOCAL_DESC] = g_param_spec_boxed ("local-description",
      "Local description", "The local SDP description", GST_TYPE_SDP_MESSAGE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_REMOTE_DESC] = g_param_spec_boxed ("remote-description",
      "Remote description", "The temote SDP description", GST_TYPE_SDP_MESSAGE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
      N_PROPERTIES, obj_properties);

  klass->add_proto_handler = kms_sdp_agent_add_proto_handler_impl;
  klass->create_offer = kms_sdp_agent_create_offer_impl;
  klass->create_answer = kms_sdp_agent_create_answer_impl;
  klass->set_local_description = kms_sdp_agent_set_local_description_impl;
  klass->set_remote_description = kms_sdp_agent_set_remote_description_impl;

  g_type_class_add_private (klass, sizeof (KmsSdpAgentPrivate));
}

static void
kms_sdp_agent_init (KmsSdpAgent * self)
{
  self->priv = KMS_SDP_AGENT_GET_PRIVATE (self);

  g_mutex_init (&self->priv->mutex);
  self->priv->medias = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_hash_table_unref);
}

KmsSdpAgent *
kms_sdp_agent_new (void)
{
  KmsSdpAgent *agent;

  agent = KMS_SDP_AGENT (g_object_new (KMS_TYPE_SDP_AGENT, NULL));

  return agent;
}

gboolean
kms_sdp_agent_add_proto_handler (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->add_proto_handler (agent, media,
      handler);
}

GstSDPMessage *
kms_sdp_agent_create_offer (KmsSdpAgent * agent, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_offer (agent, error);
}

GstSDPMessage *
kms_sdp_agent_create_answer (KmsSdpAgent * agent, const GstSDPMessage * offer)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_answer (agent, offer);
}

void
kms_sdp_agent_set_local_description (KmsSdpAgent * agent,
    GstSDPMessage * description)
{
  g_return_if_fail (KMS_IS_SDP_AGENT (agent));

  KMS_SDP_AGENT_GET_CLASS (agent)->set_local_description (agent, description);
}

void
kms_sdp_agent_set_remote_description (KmsSdpAgent * agent,
    GstSDPMessage * description)
{
  g_return_if_fail (KMS_IS_SDP_AGENT (agent));

  KMS_SDP_AGENT_GET_CLASS (agent)->set_remote_description (agent, description);
}
