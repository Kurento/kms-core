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

#include "kmssdpcontext.h"
#include "kmssdpagent.h"
#include "sdp_utils.h"

#define PLUGIN_NAME "sdpagent"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_agent_debug_category);
#define GST_CAT_DEFAULT kms_sdp_agent_debug_category

#define parent_class kms_sdp_agent_parent_class

#define DEFAULT_USE_IPV6 FALSE
#define DEFAULT_BUNDLE FALSE

#define ORIGIN_ATTR_NETTYPE "IN"
#define ORIGIN_ATTR_ADDR_TYPE_IP4 "IP4"
#define ORIGIN_ATTR_ADDR_TYPE_IP6 "IP6"
#define DEFAULT_IP4_ADDR "0.0.0.0"
#define DEFAULT_IP6_ADDR "::"
#define DEFAULT_ADDR DEFAULT_IP4_ADDR

/* Object properties */
enum
{
  PROP_0,
  PROP_USE_IPV6,
  PROP_ADDR,
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

/* Configure media callback begin */
typedef struct _KmsSdpAgentConfigureMediaCallbackData
{
  KmsSdpAgentConfigureMediaCallback callback;
  gpointer user_data;
  GDestroyNotify destroy;
} KmsSdpAgentConfigureMediaCallbackData;

static KmsSdpAgentConfigureMediaCallbackData
    * kms_sdp_agent_configure_media_callback_data_new
    (KmsSdpAgentConfigureMediaCallback callback, gpointer user_data,
    GDestroyNotify destroy)
{
  KmsSdpAgentConfigureMediaCallbackData *data;

  data = g_slice_new0 (KmsSdpAgentConfigureMediaCallbackData);
  data->callback = callback;
  data->user_data = user_data;
  data->destroy = destroy;

  return data;
}

static void
    kms_sdp_agent_configure_media_callback_data_clear
    (KmsSdpAgentConfigureMediaCallbackData ** data)
{
  if (*data == NULL) {
    return;
  }

  if ((*data)->destroy) {
    (*data)->destroy ((*data)->user_data);
  }

  g_slice_free (KmsSdpAgentConfigureMediaCallbackData, *data);
  *data = NULL;
}

/* Configure media callback end */

typedef struct _SdpHandlerGroup
{
  guint id;
  GSList *handlers;
} SdpHandlerGroup;

typedef struct _SdpHandler
{
  guint id;
  gchar *media;
  KmsSdpMediaHandler *handler;
  gboolean disabled;
} SdpHandler;

struct _KmsSdpAgentPrivate
{
  SdpMessageContext *local_description;
  GstSDPMessage *remote_description;
  gboolean use_ipv6;
  gchar *addr;

  GSList *handlers;
  GSList *groups;

  guint hids;                   /* handler ids */
  guint gids;                   /* group ids */

  KmsSdpAgentConfigureMediaCallbackData *configure_media_callback_data;

  GRecMutex mutex;

  GstSDPMessage *prev_sdp;
};

#define SDP_AGENT_LOCK(agent) \
  (g_rec_mutex_lock (&KMS_SDP_AGENT ((agent))->priv->mutex))
#define SDP_AGENT_UNLOCK(agent) \
  (g_rec_mutex_unlock (&KMS_SDP_AGENT ((agent))->priv->mutex))

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsSdpAgent, kms_sdp_agent,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_agent_debug_category, PLUGIN_NAME,
        0, "debug category for sdp agent"));

static SdpHandler *
sdp_handler_new (guint id, const gchar * media, KmsSdpMediaHandler * handler)
{
  SdpHandler *sdp_handler;

  sdp_handler = g_slice_new0 (SdpHandler);
  sdp_handler->id = id;
  sdp_handler->media = g_strdup (media);
  sdp_handler->handler = handler;

  return sdp_handler;
}

static void
sdp_handler_destroy (SdpHandler * handler)
{
  g_free (handler->media);
  g_clear_object (&handler->handler);

  g_slice_free (SdpHandler, handler);
}

static SdpHandlerGroup *
sdp_handler_group_new (guint id)
{
  SdpHandlerGroup *group;

  group = g_slice_new0 (SdpHandlerGroup);
  group->id = id;

  return group;
}

static void
sdp_handler_group_destroy (SdpHandlerGroup * group)
{
  g_slist_free (group->handlers);

  g_slice_free (SdpHandlerGroup, group);
}

static void
kms_sdp_agent_release_sdp (GstSDPMessage ** sdp)
{
  if (*sdp == NULL) {
    return;
  }

  gst_sdp_message_free (*sdp);
  *sdp = NULL;
}

static SdpHandler *
kms_sdp_agent_get_handler (KmsSdpAgent * agent, guint hid)
{
  GSList *l;

  for (l = agent->priv->handlers; l != NULL; l = l->next) {
    SdpHandler *handler = l->data;

    if (handler->id == hid) {
      return handler;
    }
  }

  return NULL;
}

static void
kms_sdp_agent_finalize (GObject * object)
{
  KmsSdpAgent *self = KMS_SDP_AGENT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  kms_sdp_agent_configure_media_callback_data_clear (&self->
      priv->configure_media_callback_data);

  if (self->priv->local_description != NULL) {
    kms_sdp_message_context_unref (self->priv->local_description);
  }

  if (self->priv->prev_sdp != NULL) {
    gst_sdp_message_free (self->priv->prev_sdp);
  }

  kms_sdp_agent_release_sdp (&self->priv->remote_description);

  g_slist_free_full (self->priv->handlers,
      (GDestroyNotify) sdp_handler_destroy);
  g_slist_free_full (self->priv->groups,
      (GDestroyNotify) sdp_handler_group_destroy);

  g_rec_mutex_clear (&self->priv->mutex);
  g_free (self->priv->addr);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_sdp_agent_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsSdpAgent *self = KMS_SDP_AGENT (object);

  SDP_AGENT_LOCK (self);

  switch (prop_id) {
    case PROP_LOCAL_DESC:{
      GError *err = NULL;
      GstSDPMessage *desc =
          kms_sdp_message_context_pack (self->priv->local_description, &err);
      if (err != NULL) {
        GST_WARNING_OBJECT (self, "Cannot get local description (%s)",
            err->message);
        g_error_free (err);
        break;
      }
      g_value_take_boxed (value, desc);
      break;
    }
    case PROP_REMOTE_DESC:
      g_value_set_boxed (value, self->priv->remote_description);
      break;
    case PROP_USE_IPV6:
      g_value_set_boolean (value, self->priv->use_ipv6);
      break;
    case PROP_ADDR:
      g_value_set_string (value, self->priv->addr);
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
    case PROP_USE_IPV6:
      self->priv->use_ipv6 = g_value_get_boolean (value);
      if (g_strcmp0 (self->priv->addr, DEFAULT_ADDR) == 0) {
        g_free (self->priv->addr);
        if (self->priv->use_ipv6) {
          self->priv->addr = g_strdup (DEFAULT_IP6_ADDR);
        } else {
          self->priv->addr = g_strdup (DEFAULT_IP4_ADDR);
        }
      }
      break;
    case PROP_ADDR:
      g_free (self->priv->addr);
      self->priv->addr = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  SDP_AGENT_UNLOCK (self);
}

static void
kms_sdp_agent_origin_init (KmsSdpAgent * agent, GstSDPOrigin * o,
    gchar * sess_id, gchar * sess_version)
{
  SdpIPv ipv;

  ipv = (agent->priv->use_ipv6) ? IPV6 : IPV4;

  o->username = "-";
  o->sess_id = sess_id;
  o->sess_version = sess_version;
  o->nettype = "IN";
  o->addrtype = (gchar *) kms_sdp_message_context_ipv2str (ipv);
  o->addr = agent->priv->addr;
}

static gint
is_disabled_handler (SdpHandler * handler, gconstpointer * data)
{
  if (handler->disabled) {
    return 0;
  } else {
    return -1;
  }
}

static void
kms_sdp_agent_add_sdp_handler (KmsSdpAgent * agent, SdpHandler * handler)
{
  GSList *l;

  l = g_slist_find_custom (agent->priv->handlers, NULL,
      (GCompareFunc) is_disabled_handler);

  if (l == NULL) {
    /* No inactive handlers */
    agent->priv->handlers = g_slist_append (agent->priv->handlers, handler);
  } else {
    SdpHandler *old_handler = l->data;

    /* rfc3264 [8.1] */
    /* New media streams are created by new additional media descriptions */
    /* below the existing ones, or by reusing the "slot" used by an old   */
    /* media stream which had been disabled by setting its port to zero.  */
    sdp_handler_destroy (old_handler);
    l->data = handler;
  }
}

static gint
kms_sdp_agent_add_proto_handler_impl (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  SdpHandler *sdp_handler;
  const gchar *addr_type;
  gchar *proto;
  gint id = -1;

  g_object_get (handler, "proto", &proto, NULL);

  if (proto == NULL) {
    GST_WARNING_OBJECT (agent, "Handler's proto can't be NULL");
    return -1;
  }

  g_free (proto);

  SDP_AGENT_LOCK (agent);

  id = agent->priv->hids++;
  sdp_handler = sdp_handler_new (id, media, handler);

  kms_sdp_agent_add_sdp_handler (agent, sdp_handler);

  if (agent->priv->use_ipv6) {
    addr_type = ORIGIN_ATTR_ADDR_TYPE_IP6;
  } else {
    addr_type = ORIGIN_ATTR_ADDR_TYPE_IP4;
  }

  g_object_set (handler, "addr-type", addr_type, "addr", agent->priv->addr,
      NULL);

  SDP_AGENT_UNLOCK (agent);

  return id;
}

static void
kms_sdp_agent_remove_handler_from_groups (KmsSdpAgent * agent,
    SdpHandler * handler)
{
  GSList *l;

  for (l = agent->priv->groups; l != NULL; l = l->next) {
    SdpHandlerGroup *group = l->data;

    kms_sdp_agent_remove_handler_from_group (agent, group->id, handler->id);
  }
}

gboolean
kms_sdp_agent_remove_proto_handler (KmsSdpAgent * agent, gint hid)
{
  SdpHandler *sdp_handler;
  gboolean ret = TRUE;

  SDP_AGENT_LOCK (agent);

  sdp_handler = kms_sdp_agent_get_handler (agent, hid);

  if (sdp_handler != NULL) {
    if (agent->priv->local_description == NULL) {
      /* No previous offer generated so we can just remove the handler */
      agent->priv->handlers = g_slist_remove (agent->priv->handlers,
          sdp_handler);
      sdp_handler_destroy (sdp_handler);
    } else {
      /* Desactive handler */
      sdp_handler->disabled = TRUE;
    }

    kms_sdp_agent_remove_handler_from_groups (agent, sdp_handler);
  } else {
    ret = FALSE;
  }

  SDP_AGENT_UNLOCK (agent);

  return ret;
}

struct SdpOfferData
{
  SdpMessageContext *ctx;
  KmsSdpAgent *agent;
};

static void
sdp_media_set_removed (GstSDPMedia ** media)
{
  GstSDPMedia *removed;

  gst_sdp_media_new (&removed);
  gst_sdp_media_set_media (removed, gst_sdp_media_get_media (*media));
  gst_sdp_media_set_proto (removed, gst_sdp_media_get_proto (*media));
  gst_sdp_media_set_port_info (removed, 0, 0);

  gst_sdp_media_free (*media);

  *media = removed;
}

static void
create_media_offers (SdpHandler * sdp_handler, struct SdpOfferData *data)
{
  SdpMediaConfig *m_conf;
  GstSDPMedia *media;
  GError *err = NULL;
  GSList *l;

  media = kms_sdp_media_handler_create_offer (sdp_handler->handler,
      sdp_handler->media, &err);

  if (err != NULL) {
    GST_ERROR_OBJECT (sdp_handler->handler, "%s", err->message);
    g_error_free (err);
    return;
  }

  if (sdp_handler->disabled) {
    sdp_media_set_removed (&media);
  }

  m_conf = kms_sdp_message_context_add_media (data->ctx, media, &err);
  if (m_conf == NULL) {
    GST_ERROR_OBJECT (sdp_handler->handler, "%s", err->message);
    g_error_free (err);
    return;
  }

  for (l = data->agent->priv->groups; l != NULL; l = l->next) {
    SdpHandlerGroup *group = l->data;
    GSList *ll;

    for (ll = group->handlers; ll != NULL; ll = ll->next) {
      SdpHandler *h = ll->data;
      SdpMediaGroup *m_group;

      if (sdp_handler->id != h->id) {
        continue;
      }

      m_group = kms_sdp_message_context_get_group (data->ctx, group->id);
      if (m_group == NULL) {
        m_group = kms_sdp_message_context_create_group (data->ctx, group->id);
      }

      if (!kms_sdp_message_context_add_media_to_group (m_group, m_conf, &err)) {
        GST_ERROR_OBJECT (sdp_handler->handler, "%s", err->message);
        g_error_free (err);
        return;
      }
    }
  }

  if (data->agent->priv->configure_media_callback_data != NULL) {
    data->agent->priv->configure_media_callback_data->callback (data->agent,
        m_conf, data->agent->priv->configure_media_callback_data->user_data);
  }
}

static guint64
get_ntp_time ()
{
  return time (NULL) + G_GUINT64_CONSTANT (2208988800);
}

static gboolean
increment_sess_version (KmsSdpAgent * agent, SdpMessageContext * new_offer,
    GError ** error)
{
  guint64 sess_version;
  gchar *str_version;
  const GstSDPOrigin *orig;
  GstSDPOrigin new_orig;
  gboolean ret;

  orig = kms_sdp_message_context_get_origin (agent->priv->local_description);

  sess_version = g_ascii_strtoull (orig->sess_version, NULL, 10);
  str_version = g_strdup_printf ("%" G_GUINT64_FORMAT, ++sess_version);

  new_orig.username = orig->username;
  new_orig.sess_id = orig->sess_id;
  new_orig.sess_version = str_version;
  new_orig.nettype = orig->nettype;
  new_orig.addrtype = orig->addrtype;
  new_orig.addr = orig->addr;

  ret = kms_sdp_message_context_set_origin (new_offer, &new_orig, error);
  g_free (str_version);

  return ret;
}

static gboolean
kms_sdp_agent_update_session_version (KmsSdpAgent * agent,
    SdpMessageContext * new_offer, GError ** error)
{
  gchar *prev_sdp_str = NULL, *new_sdp_str = NULL;
  GstSDPMessage *new_sdp;
  gboolean ret = TRUE;

  /* rfc3264 8 Modifying the Session:                                         */
  /* When issuing an offer that modifies the session, the "o=" line of the    */
  /* new SDP MUST be identical to that in the previous SDP, except that the   */
  /* version in the origin field MUST increment by one from the previous SDP. */

  if (agent->priv->prev_sdp == NULL) {
    return TRUE;
  }

  new_sdp = kms_sdp_message_context_pack (new_offer, error);
  if (new_sdp == NULL) {
    ret = FALSE;
    goto end;
  }

  prev_sdp_str = gst_sdp_message_as_text (agent->priv->prev_sdp);
  new_sdp_str = gst_sdp_message_as_text (new_sdp);

  gst_sdp_message_free (new_sdp);

  if (g_strcmp0 (prev_sdp_str, new_sdp_str) == 0) {
    /* Same offer, not updated version */
    goto end;
  }

  GST_DEBUG_OBJECT (agent, "Updating sdp session version");

  ret = increment_sess_version (agent, new_offer, error);

end:
  g_free (prev_sdp_str);
  g_free (new_sdp_str);

  return ret;
}

static SdpMessageContext *
kms_sdp_agent_create_offer_impl (KmsSdpAgent * agent, GError ** error)
{
  gchar *sess_id, *sess_version;
  struct SdpOfferData data;
  SdpMessageContext *ctx;
  GstSDPOrigin o;
  gchar *ntp = NULL;

  SDP_AGENT_LOCK (agent);

  ctx = kms_sdp_message_context_new (error);
  if (ctx == NULL) {
    SDP_AGENT_UNLOCK (agent);
    return NULL;
  }

  if (agent->priv->local_description != NULL) {
    const GstSDPOrigin *orig;

    orig = kms_sdp_message_context_get_origin (agent->priv->local_description);
    sess_id = orig->sess_id;
    sess_version = orig->sess_version;
  } else {
    /* The method of generating <sess-id> and <sess-version> is up to the    */
    /* creating tool, but it has been suggested that a Network Time Protocol */
    /* (NTP) format timestamp be used to ensure uniqueness [rfc4566] 5.2     */
    ntp = g_strdup_printf ("%" G_GUINT64_FORMAT, get_ntp_time ());
    sess_id = sess_version = ntp;
  }

  kms_sdp_agent_origin_init (agent, &o, sess_id, sess_version);

  if (!kms_sdp_message_context_set_origin (ctx, &o, error)) {
    SDP_AGENT_UNLOCK (agent);
    kms_sdp_message_context_unref (ctx);
    g_free (ntp);
    return NULL;
  }

  g_free (ntp);

  kms_sdp_message_context_set_type (ctx, KMS_SDP_OFFER);

  data.ctx = ctx;
  data.agent = agent;

  g_slist_foreach (agent->priv->handlers, (GFunc) create_media_offers, &data);

  if (!kms_sdp_agent_update_session_version (agent, ctx, error)) {
    kms_sdp_message_context_unref (ctx);
    ctx = NULL;
  }

  SDP_AGENT_UNLOCK (agent);

  return ctx;
}

struct SdpAnswerData
{
  KmsSdpAgent *agent;
  SdpMessageContext *ctx;
  GError **err;
};

static GstSDPMedia *
reject_media_answer (const GstSDPMedia * offered)
{
  GstSDPMedia *media;
  const gchar *mid;
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

  /* [rfc5888] mid attribute must be present in answer as well */
  mid = gst_sdp_media_get_attribute_val (offered, "mid");
  if (mid == NULL) {
    return media;
  }

  gst_sdp_media_add_attribute (media, INACTIVE_STR, "");
  gst_sdp_media_add_attribute (media, "mid", mid);

  return media;
}

static SdpHandler *
kms_sdp_agent_get_handler_for_media (KmsSdpAgent * agent,
    const GstSDPMedia * media)
{
  GSList *l;

  for (l = agent->priv->handlers; l != NULL; l = l->next) {
    SdpHandler *sdp_handler;

    sdp_handler = l->data;

    if (g_strcmp0 (sdp_handler->media, gst_sdp_media_get_media (media)) != 0) {
      /* This handler can not manage this media */
      continue;
    }

    if (!kms_sdp_media_handler_manage_protocol (sdp_handler->handler,
            gst_sdp_media_get_proto (media))) {
      continue;
    }

    return sdp_handler;
  }

  return NULL;
}

static gboolean
create_media_answer (const GstSDPMedia * media, struct SdpAnswerData *data)
{
  KmsSdpAgent *agent = data->agent;
  GstSDPMedia *answer_media = NULL;
  SdpHandler *sdp_handler;
  GError **err = data->err;

  SDP_AGENT_LOCK (agent);

  sdp_handler = kms_sdp_agent_get_handler_for_media (agent, media);

  SDP_AGENT_UNLOCK (agent);

  if (sdp_handler == NULL) {
    GST_WARNING_OBJECT (agent,
        "No handler for '%s' media proto '%s' found",
        gst_sdp_media_get_media (media), gst_sdp_media_get_proto (media));
    goto answer;
  }

  answer_media = kms_sdp_media_handler_create_answer (sdp_handler->handler,
      data->ctx, media, err);

  if (answer_media == NULL) {
    return FALSE;
  }

answer:
  {
    SdpMediaConfig *mconf;
    gboolean do_call = TRUE;

    if (answer_media == NULL) {
      answer_media = reject_media_answer (media);
      do_call = FALSE;
    }

    mconf = kms_sdp_message_context_add_media (data->ctx, answer_media, err);
    if (mconf == NULL) {
      return FALSE;
    }

    if (do_call && data->agent->priv->configure_media_callback_data != NULL) {
      data->agent->priv->configure_media_callback_data->callback (data->agent,
          mconf, data->agent->priv->configure_media_callback_data->user_data);
    }

    return TRUE;
  }
}

static SdpMessageContext *
kms_sdp_agent_create_answer_impl (KmsSdpAgent * agent,
    const GstSDPMessage * offer, GError ** error)
{
  gchar *sess_id, *sess_version;
  struct SdpAnswerData data;
  SdpMessageContext *ctx;
  gboolean bundle;
  const GstSDPOrigin *offer_orig;
  GstSDPOrigin o;

  offer_orig = gst_sdp_message_get_origin (offer);
  sess_id = offer_orig->sess_id;
  sess_version = offer_orig->sess_version;

  SDP_AGENT_LOCK (agent);

  kms_sdp_agent_origin_init (agent, &o, sess_id, sess_version);
  bundle = g_slist_length (agent->priv->groups) > 0;

  SDP_AGENT_UNLOCK (agent);

  ctx = kms_sdp_message_context_new (error);
  if (ctx == NULL) {
    return NULL;
  }

  if (!kms_sdp_message_context_set_origin (ctx, &o, error)) {
    kms_sdp_message_context_unref (ctx);
    return NULL;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_ANSWER);

  if (bundle
      && !kms_sdp_message_context_parse_groups_from_offer (ctx, offer, error)) {
    goto error;
  }

  if (!kms_sdp_message_context_set_common_session_attributes (ctx, offer,
          error)) {
    goto error;
  }

  data.agent = agent;
  data.ctx = ctx;
  data.err = error;

  if (!sdp_utils_for_each_media (offer, (GstSDPMediaFunc) create_media_answer,
          &data)) {
    goto error;
  }

  return ctx;

error:
  kms_sdp_message_context_unref (ctx);

  return NULL;
}

static void
kms_sdp_agent_set_local_description_impl (KmsSdpAgent * agent,
    SdpMessageContext * description)
{
  GError *err = NULL;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->local_description != NULL) {
    kms_sdp_message_context_unref (agent->priv->local_description);
  }

  agent->priv->local_description = description;

  if (agent->priv->prev_sdp != NULL) {
    gst_sdp_message_free (agent->priv->prev_sdp);
  }

  agent->priv->prev_sdp =
      kms_sdp_message_context_pack (agent->priv->local_description, &err);

  if (agent->priv->prev_sdp == NULL) {
    GST_ERROR_OBJECT (agent, "%s", err->message);
    g_error_free (err);
  }

  SDP_AGENT_UNLOCK (agent);
}

static void
kms_sdp_agent_set_remote_description_impl (KmsSdpAgent * agent,
    GstSDPMessage * description)
{
  /* TODO: */
}

gint
kms_sdp_agent_create_bundle_group_impl (KmsSdpAgent * agent)
{
  SdpHandlerGroup *group;
  guint id;

  SDP_AGENT_LOCK (agent);

  id = agent->priv->gids++;
  group = sdp_handler_group_new (id);
  agent->priv->groups = g_slist_append (agent->priv->groups, group);

  SDP_AGENT_UNLOCK (agent);

  return id;
}

static SdpHandlerGroup *
kms_sdp_agent_get_group (KmsSdpAgent * agent, guint gid)
{
  GSList *l;

  for (l = agent->priv->groups; l != NULL; l = l->next) {
    SdpHandlerGroup *group = l->data;

    if (group->id == gid) {
      return group;
    }
  }

  return NULL;
}

gboolean
kms_sdp_agent_add_handler_to_group_impl (KmsSdpAgent * agent, guint gid,
    guint hid)
{
  SdpHandlerGroup *group;
  SdpHandler *handler;
  gboolean ret = FALSE;
  GSList *l;

  SDP_AGENT_LOCK (agent);

  group = kms_sdp_agent_get_group (agent, gid);
  if (group == NULL) {
    goto end;
  }

  handler = kms_sdp_agent_get_handler (agent, hid);
  if (handler == NULL) {
    goto end;
  }

  ret = TRUE;
  for (l = group->handlers; l != NULL; l = l->next) {
    SdpHandler *h = l->data;

    if (h->id == hid) {
      goto end;
    }
  }

  group->handlers = g_slist_append (group->handlers, handler);

end:
  SDP_AGENT_UNLOCK (agent);

  return ret;
}

gboolean
kms_sdp_agent_remove_handler_from_group_impl (KmsSdpAgent * agent, guint gid,
    guint hid)
{
  SdpHandlerGroup *group;
  SdpMediaGroup *m_group;
  SdpHandler *handler;
  gboolean ret = FALSE;
  gint index;

  SDP_AGENT_LOCK (agent);

  group = kms_sdp_agent_get_group (agent, gid);
  if (group == NULL) {
    goto end;
  }

  handler = kms_sdp_agent_get_handler (agent, hid);
  if (handler == NULL) {
    goto end;
  }

  if (!g_slist_find (group->handlers, handler)) {
    goto end;
  }

  group->handlers = g_slist_remove (group->handlers, handler);
  ret = TRUE;

  if (!agent->priv->local_description) {
    goto end;
  }

  m_group = kms_sdp_message_context_get_group (agent->priv->local_description,
      group->id);
  if (m_group == NULL) {
    goto end;
  }

  index = g_slist_index (agent->priv->handlers, handler);
  kms_sdp_message_context_remove_media_from_group (m_group, index, NULL);

end:
  SDP_AGENT_UNLOCK (agent);

  return ret;
}

static void
kms_sdp_agent_class_init (KmsSdpAgentClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = kms_sdp_agent_get_property;
  gobject_class->set_property = kms_sdp_agent_set_property;
  gobject_class->finalize = kms_sdp_agent_finalize;

  obj_properties[PROP_USE_IPV6] = g_param_spec_boolean ("use-ipv6",
      "Use ipv6 in SDPs",
      "Use ipv6 addresses in generated sdp offers and answers",
      DEFAULT_USE_IPV6, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_ADDR] = g_param_spec_string ("addr", "Address",
      "The IP address used to negotiate SDPs", DEFAULT_ADDR,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
  klass->crate_bundle_group = kms_sdp_agent_create_bundle_group_impl;
  klass->add_handler_to_group = kms_sdp_agent_add_handler_to_group_impl;
  klass->remove_handler_from_group =
      kms_sdp_agent_remove_handler_from_group_impl;
  g_type_class_add_private (klass, sizeof (KmsSdpAgentPrivate));
}

static void
kms_sdp_agent_init (KmsSdpAgent * self)
{
  self->priv = KMS_SDP_AGENT_GET_PRIVATE (self);

  g_rec_mutex_init (&self->priv->mutex);
}

KmsSdpAgent *
kms_sdp_agent_new ()
{
  KmsSdpAgent *agent;

  agent = KMS_SDP_AGENT (g_object_new (KMS_TYPE_SDP_AGENT, NULL));

  return agent;
}

/* TODO: rename to _add_media_handler */
gint
kms_sdp_agent_add_proto_handler (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), -1);

  return KMS_SDP_AGENT_GET_CLASS (agent)->add_proto_handler (agent, media,
      handler);
}

SdpMessageContext *
kms_sdp_agent_create_offer (KmsSdpAgent * agent, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_offer (agent, error);
}

SdpMessageContext *
kms_sdp_agent_create_answer (KmsSdpAgent * agent, const GstSDPMessage * offer,
    GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_answer (agent, offer, error);
}

void
kms_sdp_agent_set_local_description (KmsSdpAgent * agent,
    SdpMessageContext * description)
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

gint
kms_sdp_agent_crate_bundle_group (KmsSdpAgent * agent)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), -1);

  return KMS_SDP_AGENT_GET_CLASS (agent)->crate_bundle_group (agent);
}

gboolean
kms_sdp_agent_add_handler_to_group (KmsSdpAgent * agent, guint gid, guint hid)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->add_handler_to_group (agent, gid,
      hid);
}

gboolean
kms_sdp_agent_remove_handler_from_group (KmsSdpAgent * agent, guint gid,
    guint hid)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->remove_handler_from_group (agent, gid,
      hid);
}

void
kms_sdp_agent_set_configure_media_callback (KmsSdpAgent * agent,
    KmsSdpAgentConfigureMediaCallback callback,
    gpointer user_data, GDestroyNotify destroy)
{
  KmsSdpAgentConfigureMediaCallbackData *old_data;

  SDP_AGENT_LOCK (agent);
  old_data = agent->priv->configure_media_callback_data;
  agent->priv->configure_media_callback_data =
      kms_sdp_agent_configure_media_callback_data_new (callback, user_data,
      destroy);
  SDP_AGENT_UNLOCK (agent);

  kms_sdp_agent_configure_media_callback_data_clear (&old_data);
}
