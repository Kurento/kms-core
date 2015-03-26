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
#include "sdp_utils.h"

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

struct sdp_offer_data
{
  GstSDPMessage *offer;
  const gchar *media;
};

static void
add_media_to_offer (gchar * proto, KmsSdpMediaHandler * handler,
    struct sdp_offer_data *data)
{
  GstSDPMedia *media;
  GError *err = NULL;

  media = kms_sdp_media_handler_create_offer (handler, data->media, &err);

  if (err != NULL) {
    GST_ERROR_OBJECT (handler, "%s", err->message);
    g_error_free (err);
    return;
  }

  gst_sdp_message_add_media (data->offer, media);
  gst_sdp_media_free (media);
}

static void
create_media_offers (gchar * media, GHashTable * handlers,
    GstSDPMessage * offer)
{
  struct sdp_offer_data data = {
    .offer = offer,
    .media = media
  };

  g_hash_table_foreach (handlers, (GHFunc) add_media_to_offer, &data);
}

static GstSDPMessage *
kms_sdp_agent_create_offer_impl (KmsSdpAgent * agent, GError ** error)
{
  GstSDPMessage *offer = NULL;

  gst_sdp_message_new (&offer);
  if (!kms_sdp_agent_set_default_session_attributes (agent, offer, error)) {
    kms_sdp_agent_release_sdp (&offer);
    return NULL;
  }

  SDP_AGENT_LOCK (agent);
  g_hash_table_foreach (agent->priv->medias, (GHFunc) create_media_offers,
      offer);
  SDP_AGENT_UNLOCK (agent);

  return offer;
}

struct sdp_answer_data
{
  KmsSdpAgent *agent;
  GstSDPMessage *answer;
};

static GstSDPMedia *
reject_media_answer (const GstSDPMedia * offered)
{
  GstSDPMedia *media;
  guint i, len;

  gst_sdp_media_new (&media);

  /* [rfc3264] To reject an offered stream, the port number in the */
  /* corresponding stream in the answer MUST be set to zero. Any   */
  /* media formats listed are ignored. */

  gst_sdp_media_set_media (media, gst_sdp_media_get_media (offered));
  gst_sdp_media_set_port_info (media, 0, 1);
  gst_sdp_media_set_proto (media, gst_sdp_media_get_proto (offered));

  len = gst_sdp_media_formats_len (offered);
  for (i = 0; i < len; i++) {
    const gchar *format;

    format = gst_sdp_media_get_format (offered, i);
    gst_sdp_media_insert_format (media, i, format);
  }

  return media;
}

static gboolean
create_media_answer (const GstSDPMedia * media, struct sdp_answer_data *data)
{
  KmsSdpAgent *agent = data->agent;
  GstSDPMessage *answer = data->answer;
  GHashTable *handlers;
  GstSDPMedia *answer_media = NULL;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;

  SDP_AGENT_LOCK (agent);

  handlers =
      g_hash_table_lookup (agent->priv->medias,
      gst_sdp_media_get_media (media));

  if (handlers == NULL) {
    GST_WARNING_OBJECT (agent, "%s media not supported",
        gst_sdp_media_get_media (media));
  } else {
    handler = g_hash_table_lookup (handlers, gst_sdp_media_get_proto (media));
    if (handler == NULL) {
      GST_WARNING_OBJECT (agent,
          "No handler for %s media found for protocol %s",
          gst_sdp_media_get_media (media), gst_sdp_media_get_proto (media));
    } else {
      answer_media = kms_sdp_media_handler_create_answer (handler, media, &err);
      if (err != NULL) {
        GST_ERROR_OBJECT (handler, "%s", err->message);
        g_error_free (err);
      }
    }
  }

  SDP_AGENT_UNLOCK (agent);

  if (answer_media == NULL) {
    answer_media = reject_media_answer (media);
  }

  gst_sdp_message_add_media (answer, answer_media);
  gst_sdp_media_free (answer_media);

  return TRUE;
}

static gchar **
array_to_strvector (const GArray * array)
{
  gchar **str_array;
  guint i, n;

  n = array->len + 1;

  str_array = g_new (gchar *, n);
  str_array[n] = NULL;

  for (i = 0; i < array->len; i++) {
    gchar *val;

    val = &g_array_index (array, gchar, i);
    str_array[i] = g_strdup (val);
  }

  return str_array;
}

static void
sdp_copy_timming_attrs (const GstSDPMessage * src, GstSDPMessage * dest)
{
  guint i, n;

  n = gst_sdp_message_times_len (src);

  for (i = 0; i < n; i++) {
    const GstSDPTime *time;
    gchar **repeat;

    time = gst_sdp_message_get_time (src, i);
    repeat = array_to_strvector (time->repeat);
    gst_sdp_message_add_time (dest, time->start, time->stop,
        (const gchar **) repeat);
    g_strfreev (repeat);
  }
}

static GstSDPMessage *
kms_sdp_agent_create_answer_impl (KmsSdpAgent * agent,
    const GstSDPMessage * offer, GError ** error)
{
  GstSDPMessage *answer;
  struct sdp_answer_data data;

  gst_sdp_message_new (&answer);
  data.answer = answer;
  data.agent = agent;

  if (!kms_sdp_agent_set_default_session_attributes (agent, answer, error)) {
    kms_sdp_agent_release_sdp (&answer);
    return NULL;
  }

  /* [rfc3264] The "t=" line in the answer MUST be equal to the ones in the */
  /* offer. The time of the session cannot be negotiated. */
  if (FALSE)
    sdp_copy_timming_attrs (offer, answer);

  if (!sdp_utils_for_each_media (offer, (GstSDPMediaFunc) create_media_answer,
          &data)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        " create SDP response");
    kms_sdp_agent_release_sdp (&answer);
    return NULL;
  }

  return answer;
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
kms_sdp_agent_create_answer (KmsSdpAgent * agent, const GstSDPMessage * offer,
    GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_answer (agent, offer, error);
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
