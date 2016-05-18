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

#include "kmsrefstruct.h"
#include "kmssdpcontext.h"
#include "kmssdpagent.h"
#include "sdp_utils.h"
#include "kmssdpagentcommon.h"
#include "kmssdpgroupmanager.h"
#include "kmssdpbasegroup.h"
#include "kmssdprejectmediahandler.h"
#include "kmssdpmediadirext.h"
#include "kmssdpmidext.h"

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

#define is_internal_handler(agent, handler) \
  (!g_slist_find ((agent)->priv->handlers, handler))

/*
 * Agent state machine:
 *
 * Modification of the SDP media is only allowed when agent is not
 * negotiating any SDP, so further modifications by adding or removing
 * media will only allowed when agent is either in in UNNEGOTIATED or
 * NEGOTIATED state.
 *
 *                             +-------------+
 *                        no   |  previously |   yes
 *  +--------------------------| negotiated? |---------------------------------------+
 *  |                          +-------------+                                       |
 *  |                                 A                                              |
 *  |                                 | cancel_offer()                               |
 *  |                                 |                                              |
 *  |  create_local_offer      +-------------+  create_local_offer                   |
 *  |   +--------------------->| LOCAL_OFFER |<-----------------------------------+  |
 *  |   |                      +-------------+                                    |  |
 *  |   |                            |                                            |  |
 *  |   |                            |  set_local_description()                   |  |
 *  V   |                            V                                            |  V
 * +--+-----------+             +-----------+   set_remote_description()     +------------+
 * | UNNEGOTIATED |             | WAIT_NEGO |------------------------------->| NEGOTIATED |
 * +--------------+             +-----------+                                +------------+
 *    |                                        _______________________________^   |
 *    |                                       /     set_local_description()       |
 *    |                                      /                                    |
 *    |                          +--------------+   set_remote_description()      |
 *    +------------------------->| REMOTE_OFFER |<--------------------------------+
 *     set_remote_description()  +--------------+
 *
 */

typedef enum
{
  KMS_SDP_AGENT_STATE_UNNEGOTIATED,
  KMS_SDP_AGENT_STATE_LOCAL_OFFER,
  KMS_SDP_AGENT_STATE_REMOTE_OFFER,
  KMS_SDP_AGENT_STATE_WAIT_NEGO,
  KMS_SDP_AGENT_STATE_NEGOTIATED
} KmsSdpAgentState;

static const gchar *kms_sdp_agent_states[] = {
  "unnegotiated",
  "local_offer",
  "remote_offer",
  "wait_nego",
  "negotiated"
};

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
  KmsRefStruct ref;
  guint id;
  guint index;
  gboolean negotiated;
  KmsSdpBaseGroup *group;
  GSList *handlers;
} SdpHandlerGroup;

typedef struct _SdpHandler
{
  KmsRefStruct ref;
  KmsSdpHandler *sdph;
  gboolean disabled;
  gboolean unsupported;         /* unsupported by local agent  */
  gboolean rejected;            /* unsupported by remote agent */
  gboolean offer;
  GstSDPMedia *unsupported_media;
  GstSDPMedia *offer_media;
} SdpHandler;

typedef struct _SdpSessionDescription
{
  gchar *id;
  gchar *version;
} SdpSessionDescription;

typedef struct _KmsSdpAgentCallbacksData
{
  KmsSdpAgentCallbacks callbacks;
  gpointer user_data;
  GDestroyNotify destroy;
} KmsSdpAgentCallbacksData;

struct _KmsSdpAgentPrivate
{
  KmsSdpGroupManager *group_manager;

  SdpMessageContext *local_description;
  GstSDPMessage *remote_description;
  gboolean use_ipv6;
  gchar *addr;

  GSList *handlers;
  GSList *groups;

  guint hids;                   /* handler ids */
  guint gids;                   /* group ids */

  KmsSdpAgentConfigureMediaCallbackData *configure_media_callback_data; /* deprecated */
  KmsSdpAgentCallbacksData callbacks;

  GRecMutex mutex;

  KmsSdpAgentState state;
  GstSDPMessage *prev_sdp;
  GSList *offer_handlers;

  SdpSessionDescription local;
  SdpSessionDescription remote;

  GSList *extensions;
};

#define SDP_AGENT_STATE(agent) kms_sdp_agent_states[(agent)->priv->state]
#define SDP_AGENT_NEW_STATE(agent, new_state) do {               \
  GST_DEBUG_OBJECT ((agent), "State changed from '%s' to '%s'",  \
    SDP_AGENT_STATE (agent), kms_sdp_agent_states[(new_state)]); \
  kms_sdp_agent_commit_state_operations ((agent), new_state);    \
  (agent)->priv->state = new_state;                              \
} while (0)

#define SDP_AGENT_LOCK(agent) \
  (g_rec_mutex_lock (&KMS_SDP_AGENT ((agent))->priv->mutex))
#define SDP_AGENT_UNLOCK(agent) \
  (g_rec_mutex_unlock (&KMS_SDP_AGENT ((agent))->priv->mutex))

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsSdpAgent, kms_sdp_agent,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_agent_debug_category, PLUGIN_NAME,
        0, "debug category for sdp agent"));

static guint64
get_ntp_time ()
{
  return time (NULL) + G_GUINT64_CONSTANT (2208988800);
}

static void
clear_sdp_session_description (SdpSessionDescription * desc)
{
  g_free (desc->id);
  g_free (desc->version);

  desc->id = NULL;
  desc->version = NULL;
}

static void
set_sdp_session_description (SdpSessionDescription * desc, const gchar * id,
    const gchar * version)
{
  clear_sdp_session_description (desc);

  desc->id = g_strdup (id);
  desc->version = g_strdup (version);
}

static void
generate_sdp_session_description (SdpSessionDescription * desc)
{
  clear_sdp_session_description (desc);

  /* The method of generating <sess-id> and <sess-version> is up to the    */
  /* creating tool, but it has been suggested that a Network Time Protocol */
  /* (NTP) format timestamp be used to ensure uniqueness [rfc4566] 5.2     */

  desc->id = g_strdup_printf ("%" G_GUINT64_FORMAT, get_ntp_time ());
  desc->version = g_strdup (desc->id);
}

static void
mark_handler_as_negotiated (gpointer data, gpointer user_data)
{
  SdpHandler *handler = data;

  handler->sdph->negotiated = TRUE;
}

static gint
disable_handler_cmp_func (SdpHandler * handler, gconstpointer * data)
{
  if (handler->disabled) {
    return 0;
  } else {
    return -1;
  }
}

static void
update_media_offered (SdpHandler * handler, GstSDPMedia * media)
{
  if (handler->offer_media != NULL) {
    gst_sdp_media_free (handler->offer_media);
  }

  if (media != NULL) {
    gst_sdp_media_copy (media, &handler->offer_media);
  }
}

static void
kms_sdp_agent_remove_media_handler (KmsSdpAgent * agent, SdpHandler * handler)
{
  if (!kms_sdp_group_manager_remove_handler (agent->priv->group_manager,
          handler->sdph)) {
    GST_ERROR_OBJECT (agent, "Problems removing handler %u", handler->sdph->id);
  }

  agent->priv->handlers = g_slist_remove (agent->priv->handlers, handler);
  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (handler));
}

static void
kms_sdp_agent_remove_disabled_medias (KmsSdpAgent * agent)
{
  GSList *l;

  while ((l = g_slist_find_custom (agent->priv->handlers, NULL,
              (GCompareFunc) disable_handler_cmp_func)) != NULL) {
    kms_sdp_agent_remove_media_handler (agent, l->data);
  }
}

static void
kms_sdp_agent_commit_state_operations (KmsSdpAgent * agent,
    KmsSdpAgentState new_state)
{
  switch (new_state) {
    case KMS_SDP_AGENT_STATE_UNNEGOTIATED:
      clear_sdp_session_description (&agent->priv->local);
      clear_sdp_session_description (&agent->priv->remote);
      g_slist_free_full (agent->priv->offer_handlers,
          (GDestroyNotify) kms_ref_struct_unref);
      agent->priv->offer_handlers = NULL;
      break;
    case KMS_SDP_AGENT_STATE_NEGOTIATED:
      if (agent->priv->state != KMS_SDP_AGENT_STATE_LOCAL_OFFER) {
        /* Offer has not been canceled, so we cant mark medias as negotiated */
        g_slist_foreach (agent->priv->offer_handlers,
            mark_handler_as_negotiated, NULL);
      }

      kms_sdp_agent_remove_disabled_medias (agent);
      break;
    default:
      GST_LOG_OBJECT (agent, "Nothing to commit for state '%s'",
          kms_sdp_agent_states[new_state]);
      break;
  }
}

static void
sdp_handler_destroy (SdpHandler * handler)
{
  if (handler->unsupported_media != NULL) {
    gst_sdp_media_free (handler->unsupported_media);
  }

  if (handler->offer_media != NULL) {
    gst_sdp_media_free (handler->offer_media);
  }

  kms_sdp_agent_common_unref_sdp_handler (handler->sdph);

  g_slice_free (SdpHandler, handler);
}

static SdpHandler *
sdp_handler_new (guint id, const gchar * media, KmsSdpMediaHandler * handler)
{
  SdpHandler *sdp_handler;

  sdp_handler = g_slice_new0 (SdpHandler);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (sdp_handler),
      (GDestroyNotify) sdp_handler_destroy);

  sdp_handler->sdph = kms_sdp_agent_common_new_sdp_handler (id, media, handler);

  sdp_handler->unsupported = KMS_IS_SDP_REJECT_MEDIA_HANDLER (handler);

  return sdp_handler;
}

static void
sdp_handler_group_destroy (SdpHandlerGroup * group)
{
  g_slist_free (group->handlers);
  g_clear_object (&group->group);

  g_slice_free (SdpHandlerGroup, group);
}

static SdpHandlerGroup *
sdp_handler_group_new (guint id, KmsSdpBaseGroup * group)
{
  SdpHandlerGroup *sdp_group;

  sdp_group = g_slice_new0 (SdpHandlerGroup);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (sdp_group),
      (GDestroyNotify) sdp_handler_group_destroy);

  sdp_group->id = id;
  sdp_group->group = group;

  return sdp_group;
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

    if (handler->sdph->id == hid) {
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

  if (self->priv->callbacks.destroy != NULL &&
      self->priv->callbacks.user_data != NULL) {
    self->priv->callbacks.destroy (self->priv->callbacks.user_data);
  }

  if (self->priv->local_description != NULL) {
    kms_sdp_message_context_unref (self->priv->local_description);
  }

  if (self->priv->prev_sdp != NULL) {
    gst_sdp_message_free (self->priv->prev_sdp);
  }

  kms_sdp_agent_release_sdp (&self->priv->remote_description);

  g_slist_free_full (self->priv->extensions, g_object_unref);

  g_slist_free_full (self->priv->offer_handlers,
      (GDestroyNotify) kms_ref_struct_unref);
  g_slist_free_full (self->priv->handlers,
      (GDestroyNotify) kms_ref_struct_unref);
  g_slist_free_full (self->priv->groups, (GDestroyNotify) kms_ref_struct_unref);

  g_rec_mutex_clear (&self->priv->mutex);

  g_free (self->priv->addr);

  clear_sdp_session_description (&self->priv->local);
  clear_sdp_session_description (&self->priv->remote);

  g_clear_object (&self->priv->group_manager);

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

static SdpHandler *
kms_sdp_agent_create_media_handler (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  SdpHandler *sdp_handler;
  gint id = -1;

  id = agent->priv->hids++;
  sdp_handler = sdp_handler_new (id, media, handler);

  kms_sdp_group_manager_add_handler (agent->priv->group_manager,
      (KmsSdpHandler *)
      kms_ref_struct_ref (KMS_REF_STRUCT_CAST (sdp_handler->sdph)));

  return sdp_handler;
}

static gint
kms_sdp_agent_append_media_handler (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  GError *err = NULL;
  SdpHandler *sdp_handler;
  const gchar *addr_type;

  sdp_handler = kms_sdp_agent_create_media_handler (agent, media, handler);

  if (!kms_sdp_media_handler_set_parent (handler, agent, &err)) {
    GST_WARNING_OBJECT (agent, "%s", err->message);
    kms_sdp_agent_remove_media_handler (agent, sdp_handler);
    g_error_free (err);
    return -1;
  }

  if (!kms_sdp_media_handler_set_id (handler, sdp_handler->sdph->id, &err)) {
    GST_WARNING_OBJECT (agent, "%s", err->message);
    kms_sdp_agent_remove_media_handler (agent, sdp_handler);
    kms_sdp_media_handler_remove_parent (handler);
    g_error_free (err);
    return -1;
  }

  agent->priv->handlers = g_slist_append (agent->priv->handlers, sdp_handler);

  if (agent->priv->use_ipv6) {
    addr_type = ORIGIN_ATTR_ADDR_TYPE_IP6;
  } else {
    addr_type = ORIGIN_ATTR_ADDR_TYPE_IP4;
  }

  g_object_set (handler, "addr-type", addr_type, "addr", agent->priv->addr,
      NULL);

  return sdp_handler->sdph->id;
}

static gint
kms_sdp_agent_add_proto_handler_impl (KmsSdpAgent * agent, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  gchar *proto;
  gint id = -1;

  g_object_get (handler, "proto", &proto, NULL);

  if (proto == NULL) {
    GST_WARNING_OBJECT (agent, "Handler's proto can't be NULL");
    return -1;
  }

  g_free (proto);

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    SDP_AGENT_UNLOCK (agent);
    GST_WARNING_OBJECT (agent,
        "Can not manipulate media while negotiation is taking place (state: %s)",
        kms_sdp_agent_states[agent->priv->state]);
    return -1;
  }

  id = kms_sdp_agent_append_media_handler (agent, media, handler);

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

    kms_sdp_agent_remove_handler_from_group (agent, group->id,
        handler->sdph->id);
  }
}

gint
kms_sdp_agent_get_handler_index_impl (KmsSdpAgent * agent, gint hid)
{
  SdpHandler *sdp_handler;
  gint index = -1;

  SDP_AGENT_LOCK (agent);

  sdp_handler = kms_sdp_agent_get_handler (agent, hid);

  if (sdp_handler == NULL) {
    goto end;
  }

  if (sdp_handler->disabled) {
    /* Handler is disabled but is still required by the agent. Do not  */
    /* provide a vaid index in this case so it is not considered for   */
    /* effective media negotiation any more. */
    goto end;
  }

  index = g_slist_index (agent->priv->offer_handlers, sdp_handler);

  if (index >= 0) {
    goto end;
  }

  /* Perhaps this handler is offered but not yet negotiated so we get the */
  /* index which it has been assgined in the offer */
  if (sdp_handler->offer) {
    index = sdp_handler->sdph->index;
  }

end:
  SDP_AGENT_UNLOCK (agent);

  return index;
}

gboolean
kms_sdp_agent_remove_proto_handler (KmsSdpAgent * agent, gint hid)
{
  SdpHandler *sdp_handler;
  gboolean ret = TRUE;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    SDP_AGENT_UNLOCK (agent);
    GST_WARNING_OBJECT (agent,
        "Can not manipulate media while negotiation is taking place");
    return FALSE;
  }

  sdp_handler = kms_sdp_agent_get_handler (agent, hid);

  if (sdp_handler == NULL) {
    ret = FALSE;
    goto end;
  }

  kms_sdp_agent_remove_handler_from_groups (agent, sdp_handler);
  kms_sdp_media_handler_remove_parent (sdp_handler->sdph->handler);

  if (!sdp_handler->sdph->negotiated) {
    /* No previous offer generated so we can just remove the handler */
    kms_sdp_agent_remove_media_handler (agent, sdp_handler);
  } else {
    /* Desactive handler */
    sdp_handler->disabled = TRUE;
  }

end:
  SDP_AGENT_UNLOCK (agent);

  return ret;
}

struct SdpOfferData
{
  guint index;
  SdpMessageContext *ctx;
  GstSDPMessage *offer;
  KmsSdpAgent *agent;
};

static GstSDPMedia *
kms_sdp_agent_get_negotiated_media (KmsSdpAgent * agent,
    SdpHandler * sdp_handler, GError ** error)
{
  GstSDPMessage *desc;
  GstSDPMedia *media = NULL;
  guint index;

  index = sdp_handler->sdph->index;

  if (!sdp_handler->sdph->negotiated) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "Media handler '%s' at [%d] not negotiated", sdp_handler->sdph->media,
        index);
    return NULL;
  }

  if (sdp_handler->offer) {
    /* We offered this media. Remote description has the negotiated media */
    desc = agent->priv->remote_description;
  } else {
    /* Local description has the media negotiated */
    desc = kms_sdp_message_context_pack (agent->priv->local_description, error);
  }

  if (desc == NULL) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "No previous SDP negotiated");
    return NULL;
  }

  if (index >= gst_sdp_message_medias_len (desc)) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Could not process media '%s'", sdp_handler->sdph->media);
  } else {
    gst_sdp_media_copy (gst_sdp_message_get_media (desc, index), &media);
  }

  if (!sdp_handler->offer) {
    gst_sdp_message_free (desc);
  }

  return media;
}

static GstSDPDirection
kms_sdp_agent_on_offer_dir (KmsSdpMediaDirectionExt * ext, gpointer user_data)
{
  return INACTIVE;
}

static GstSDPDirection
kms_sdp_agent_on_answer_dir (KmsSdpMediaDirectionExt * ext,
    GstSDPDirection dir, gpointer user_data)
{
  return INACTIVE;
}

static gboolean
kms_sdp_agent_on_answer_mid (KmsISdpMediaExtension * ext, gchar * mid,
    gpointer user_data)
{
  return TRUE;
}

static KmsSdpMediaHandler *
create_reject_handler ()
{
  KmsSdpMediaDirectionExt *ext;
  KmsSdpMediaHandler *handler;

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_reject_media_handler_new ());
  ext = kms_sdp_media_direction_ext_new ();

  g_signal_connect (ext, "on-offer-media-direction",
      G_CALLBACK (kms_sdp_agent_on_offer_dir), NULL);
  g_signal_connect (ext, "on-answer-media-direction",
      G_CALLBACK (kms_sdp_agent_on_answer_dir), NULL);

  kms_sdp_media_handler_add_media_extension (handler,
      KMS_I_SDP_MEDIA_EXTENSION (ext));

  return handler;
}

static void
reject_sdp_media (GstSDPMedia ** media)
{
  KmsSdpMediaHandler *handler = create_reject_handler ();
  GstSDPMedia *rejected;
  KmsSdpMidExt *ext;

  ext = kms_sdp_mid_ext_new ();

  g_signal_connect (ext, "on-answer-mid",
      G_CALLBACK (kms_sdp_agent_on_answer_mid), NULL);

  kms_sdp_media_handler_add_media_extension (handler,
      KMS_I_SDP_MEDIA_EXTENSION (ext));

  rejected = kms_sdp_media_handler_create_answer (handler, NULL, *media, NULL);
  g_object_unref (handler);

  gst_sdp_media_free (*media);
  *media = rejected;
}

static GstSDPMedia *
kms_sdp_agent_create_proper_media_offer (KmsSdpAgent * agent,
    SdpHandler * sdp_handler, GError ** err)
{
  GstSDPMessage *desc;
  GstSDPMedia *media;
  guint index;

  if (sdp_handler->disabled) {
    /* Try to generate an offer to provide a rejected media in the offer. */
    /* Objects that use the agent could realize this is a fake offer      */
    /* checking the index attribute of the media handler */
    GST_DEBUG ("Removed negotiated media %u, %s", sdp_handler->sdph->index,
        sdp_handler->sdph->media);
    media = kms_sdp_agent_get_negotiated_media (agent, sdp_handler, err);
    if (media != NULL) {
      reject_sdp_media (&media);
    }

    return media;
  }

  if (sdp_handler->unsupported) {
    gst_sdp_media_copy (sdp_handler->unsupported_media, &media);
    return media;
  }

  media = kms_sdp_media_handler_create_offer (sdp_handler->sdph->handler,
      sdp_handler->sdph->media, err);

  if (media == NULL) {
    return NULL;
  }

  if (!sdp_handler->sdph->negotiated) {
    update_media_offered (sdp_handler, media);
    sdp_handler->offer = TRUE;
    return media;
  }

  /* Check if this handler has provided new capabilities, if so, renegotiate */
  if (!sdp_utils_equal_medias (sdp_handler->offer_media, media)) {
    sdp_handler->offer = TRUE;
    return media;
  }

  /* Handler has the same capabilities. Do not renegotiate */
  index = g_slist_index (agent->priv->offer_handlers, sdp_handler);
  gst_sdp_media_free (media);

  if (sdp_handler->offer) {
    /* We offered this media. Remote description has the negotiated media */
    desc = agent->priv->remote_description;
  } else {
    /* Local description has the media negotiated */
    desc = kms_sdp_message_context_pack (agent->priv->local_description, err);
  }

  if (desc == NULL) {
    return NULL;
  }

  if (index >= gst_sdp_message_medias_len (desc)) {
    if (!sdp_handler->sdph->negotiated && sdp_handler->offer) {
      GST_DEBUG_OBJECT (agent,
          "Media '%s' at %u canceled before finishing negotiation",
          gst_sdp_media_get_media (sdp_handler->offer_media), index);
      gst_sdp_media_copy (sdp_handler->offer_media, &media);
      goto end;
    }
    g_set_error (err, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Could not process media '%s'", sdp_handler->sdph->media);
    return NULL;
  }

  gst_sdp_media_copy (gst_sdp_message_get_media (desc, index), &media);

  if (!sdp_handler->offer) {
    /* Free packed SDP message */
    gst_sdp_message_free (desc);
  }

end:
  if (g_strcmp0 (gst_sdp_media_get_media (media),
          sdp_handler->sdph->media) != 0) {
    g_set_error (err, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Mismatching media '%s' for handler", sdp_handler->sdph->media);
    gst_sdp_media_free (media);
    return NULL;
  }

  return media;
}

static void
kms_sdp_agent_fire_on_offer_callback (KmsSdpAgent * agent,
    KmsSdpMediaHandler * handler, SdpMediaConfig * mconf)
{
  /* deprecated */
  if (agent->priv->configure_media_callback_data != NULL) {
    agent->priv->configure_media_callback_data->callback (agent, handler, mconf,
        agent->priv->configure_media_callback_data->user_data);
  }

  if (agent->priv->callbacks.callbacks.on_media_offer != NULL) {
    agent->priv->callbacks.callbacks.on_media_offer (agent, handler, mconf,
        agent->priv->callbacks.user_data);
  }
}

static void
kms_sdp_agent_fire_on_answer_callback (KmsSdpAgent * agent,
    KmsSdpMediaHandler * handler, SdpMediaConfig * mconf)
{
  /* deprecated */
  if (agent->priv->configure_media_callback_data != NULL) {
    agent->priv->configure_media_callback_data->callback (agent, handler, mconf,
        agent->priv->configure_media_callback_data->user_data);
  }

  if (agent->priv->callbacks.callbacks.on_media_answer != NULL) {
    agent->priv->callbacks.callbacks.on_media_answer (agent, handler, mconf,
        agent->priv->callbacks.user_data);
  }
}

static void
create_media_offers (SdpHandler * sdp_handler, struct SdpOfferData *data)
{
  SdpMediaConfig *m_conf;
  GstSDPMedia *media;
  GError *err = NULL;
  GSList *l;

  media =
      kms_sdp_agent_create_proper_media_offer (data->agent, sdp_handler, &err);

  if (err != NULL) {
    GST_ERROR_OBJECT (sdp_handler->sdph->handler, "%s", err->message);
    g_error_free (err);
    return;
  }

  /* update index */
  sdp_handler->sdph->index = data->index++;

  m_conf = kms_sdp_message_context_add_media (data->ctx, media, &err);
  if (m_conf == NULL) {
    GST_ERROR_OBJECT (sdp_handler->sdph->handler, "%s", err->message);
    g_error_free (err);
    return;
  }

  for (l = data->agent->priv->groups; l != NULL; l = l->next) {
    SdpHandlerGroup *group = l->data;
    GSList *ll;

    for (ll = group->handlers; ll != NULL; ll = ll->next) {
      SdpHandler *h = ll->data;
      SdpMediaGroup *m_group;

      if (sdp_handler->sdph->id != h->sdph->id) {
        continue;
      }

      m_group = kms_sdp_message_context_get_group (data->ctx, group->id);
      if (m_group == NULL) {
        m_group = kms_sdp_message_context_create_group (data->ctx, group->id);
      }

      if (!kms_sdp_message_context_add_media_to_group (m_group, m_conf, &err)) {
        GST_ERROR_OBJECT (sdp_handler->sdph->handler, "%s", err->message);
        g_error_free (err);
        return;
      }
    }
  }

  kms_sdp_agent_fire_on_offer_callback (data->agent, sdp_handler->sdph->handler,
      m_conf);
}

static gboolean
increment_sess_version (KmsSdpAgent * agent, SdpMessageContext * new_offer,
    GError ** error)
{
  guint64 sess_version;
  const GstSDPOrigin *orig;
  GstSDPOrigin new_orig;
  gboolean ret;

  orig = kms_sdp_message_context_get_origin (agent->priv->local_description);

  sess_version = g_ascii_strtoull (orig->sess_version, NULL, 10);

  g_free (agent->priv->local.version);

  agent->priv->local.version =
      g_strdup_printf ("%" G_GUINT64_FORMAT, ++sess_version);

  new_orig.username = orig->username;
  new_orig.sess_id = orig->sess_id;
  new_orig.sess_version = agent->priv->local.version;
  new_orig.nettype = orig->nettype;
  new_orig.addrtype = orig->addrtype;
  new_orig.addr = orig->addr;

  ret = kms_sdp_message_context_set_origin (new_offer, &new_orig, error);

  return ret;
}

static gboolean
kms_sdp_agent_update_session_version (KmsSdpAgent * agent,
    SdpMessageContext * new_offer, GError ** error)
{
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

  if (!sdp_utils_equal_messages (agent->priv->prev_sdp, new_sdp)) {
    GST_DEBUG_OBJECT (agent, "Updating sdp session version");
    ret = increment_sess_version (agent, new_offer, error);
  }

end:
  if (new_sdp != NULL) {
    gst_sdp_message_free (new_sdp);
  }

  return ret;
}

static gpointer
sdp_handler_ref (SdpHandler * sdp_handler, gpointer user_data)
{
  return kms_ref_struct_ref (KMS_REF_STRUCT_CAST (sdp_handler));
}

static gint
handler_cmp_func (SdpHandler * h1, SdpHandler * h2)
{
  return h1->sdph->id - h2->sdph->id;
}

static void
kms_sdp_agent_merge_handler_func (SdpHandler * handler, KmsSdpAgent * agent)
{
  GSList *l;

  if (handler->sdph->negotiated || handler->disabled) {
    /* This handler is already in the offer */
    return;
  }

  l = g_slist_find_custom (agent->priv->offer_handlers, handler,
      (GCompareFunc) handler_cmp_func);

  if (l == NULL) {
    /* This handler is not yet in the offer */
    GST_DEBUG ("Adding handler %u", handler->sdph->id);
    agent->priv->offer_handlers = g_slist_append (agent->priv->offer_handlers,
        kms_ref_struct_ref (KMS_REF_STRUCT_CAST (handler)));
  }
}

static SdpHandler *
kms_sdp_agent_get_first_not_negotiated_handler (KmsSdpAgent * agent)
{
  GSList *l;

  for (l = agent->priv->handlers; l != NULL; l = g_slist_next (l)) {
    SdpHandler *handler;

    handler = l->data;

    if (handler->sdph->negotiated) {
      continue;
    }

    if (g_slist_find_custom (agent->priv->offer_handlers, handler,
            (GCompareFunc) handler_cmp_func) == NULL) {
      /* This handler is not yet added */
      return handler;
    }
  }

  return NULL;
}

static gint
reusable_slot_cmp (SdpHandler * handler, gconstpointer * data)
{
  if (handler->disabled || handler->unsupported) {
    return 0;
  } else {
    return -1;
  }
}

static void
kms_sdp_agent_merge_offer_handlers (KmsSdpAgent * agent)
{
  GSList *l;

  if (agent->priv->state == KMS_SDP_AGENT_STATE_UNNEGOTIATED) {
    /* No preivous offer generated */
    agent->priv->offer_handlers = g_slist_copy_deep (agent->priv->handlers,
        (GCopyFunc) sdp_handler_ref, NULL);
    return;
  }

  while ((l = g_slist_find_custom (agent->priv->offer_handlers, NULL,
              (GCompareFunc) reusable_slot_cmp)) != NULL) {
    SdpHandler *old_handler, *new_handler;

    new_handler = kms_sdp_agent_get_first_not_negotiated_handler (agent);

    if (new_handler == NULL) {
      /* No more new handlers available to fix slots */
      break;
    }

    /* rfc3264 [8.1] */
    /* New media streams are created by new additional media descriptions */
    /* below the existing ones, or by reusing the "slot" used by an old   */
    /* media stream which had been disabled by setting its port to zero.  */

    old_handler = l->data;
    kms_ref_struct_unref (KMS_REF_STRUCT_CAST (old_handler));
    l->data = kms_ref_struct_ref (KMS_REF_STRUCT_CAST (new_handler));
  }

  /* Add the rest of new handlers */
  g_slist_foreach (agent->priv->handlers,
      (GFunc) kms_sdp_agent_merge_handler_func, agent);
}

static void
pre_processing_extensions (KmsISdpSessionExtension * ext,
    struct SdpOfferData *data)
{
  GstSDPMessage *offer;
  GError *err = NULL;
  gboolean pre_proc;

  g_object_get (ext, "pre-media-processing", &pre_proc, NULL);

  if (!pre_proc) {
    /* this extension should be executed at the end */
    return;
  }

  offer = kms_sdp_message_context_get_sdp_message (data->ctx);

  if (!kms_i_sdp_session_extension_add_offer_attributes (ext, offer, &err)) {
    GST_ERROR_OBJECT (data->agent, "%s", err->message);
  }

  g_clear_error (&err);
}

static void
post_processing_extensions (KmsISdpSessionExtension * ext,
    struct SdpOfferData *data)
{
  GError *err = NULL;
  gboolean pre_proc;

  g_object_get (ext, "pre-media-processing", &pre_proc, NULL);

  if (pre_proc) {
    /* this extension has been executed already */
    return;
  }

  if (!kms_i_sdp_session_extension_add_offer_attributes (ext, data->offer,
          &err)) {
    GST_ERROR_OBJECT (data->agent, "%s", err->message);
  }

  g_clear_error (&err);
}

static GstSDPMessage *
kms_sdp_agent_create_offer_impl (KmsSdpAgent * agent, GError ** error)
{
  SdpMessageContext *ctx = NULL;
  GstSDPMessage *offer = NULL;
  struct SdpOfferData data;
  gchar *ntp = NULL;
  GstSDPOrigin o;
  GSList *tmp;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "Agent in state %s", SDP_AGENT_STATE (agent));
    goto end;
  }

  ctx = kms_sdp_message_context_new (error);
  if (ctx == NULL) {
    goto end;
  }

  if (agent->priv->state == KMS_SDP_AGENT_STATE_NEGOTIATED) {
    const GstSDPOrigin *orig;

    orig = kms_sdp_message_context_get_origin (agent->priv->local_description);
    set_sdp_session_description (&agent->priv->local, orig->sess_id,
        orig->sess_version);
  } else {
    generate_sdp_session_description (&agent->priv->local);
  }

  kms_sdp_agent_origin_init (agent, &o, agent->priv->local.id,
      agent->priv->local.version);

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
  data.index = 0;

  /* Execute pre-processing extensions */
  g_slist_foreach (agent->priv->extensions,
      (GFunc) pre_processing_extensions, &data);

  tmp = g_slist_copy_deep (agent->priv->offer_handlers,
      (GCopyFunc) sdp_handler_ref, NULL);
  kms_sdp_agent_merge_offer_handlers (agent);

  /* Process medias */
  g_slist_foreach (agent->priv->offer_handlers, (GFunc) create_media_offers,
      &data);

  if (!kms_sdp_agent_update_session_version (agent, ctx, error)) {
    kms_sdp_message_context_unref (ctx);
    g_slist_free_full (agent->priv->offer_handlers,
        (GDestroyNotify) kms_ref_struct_unref);
    agent->priv->offer_handlers = tmp;
    ctx = NULL;
  } else {
    g_slist_free_full (tmp, (GDestroyNotify) kms_ref_struct_unref);
    SDP_AGENT_NEW_STATE (agent, KMS_SDP_AGENT_STATE_LOCAL_OFFER);
  }

  offer = kms_sdp_message_context_pack (ctx, error);

  if (offer == NULL) {
    goto end;
  }

  data.offer = offer;

  /* Execute post-processing extensions */
  g_slist_foreach (agent->priv->extensions,
      (GFunc) post_processing_extensions, &data);

end:
  SDP_AGENT_UNLOCK (agent);

  if (ctx != NULL) {
    kms_sdp_message_context_unref (ctx);
  }

  return offer;
}

struct SdpAnswerData
{
  guint index;
  KmsSdpAgent *agent;
  SdpMessageContext *ctx;
  const GstSDPMessage *offer;
  GError **err;
};

static SdpHandler *
kms_sdp_agent_request_handler (KmsSdpAgent * agent, const GstSDPMedia * media)
{
  KmsSdpMediaHandler *handler;
  gchar *proto;
  gint hid;

  if (agent->priv->callbacks.callbacks.on_handler_required == NULL) {
    return NULL;
  }

  handler = agent->priv->callbacks.callbacks.on_handler_required (agent, media,
      agent->priv->callbacks.user_data);

  if (handler == NULL) {
    return NULL;
  }

  g_object_get (handler, "proto", &proto, NULL);

  if (proto == NULL) {
    GST_WARNING_OBJECT (agent, "Handler's proto can't be NULL");
    g_object_unref (handler);
    return NULL;
  }

  g_free (proto);

  if (!kms_sdp_media_handler_manage_protocol (handler,
          gst_sdp_media_get_proto (media))) {
    GST_WARNING_OBJECT (agent, "Handler can not manage media: %s",
        gst_sdp_media_get_proto (media));
    g_object_unref (handler);
    return NULL;
  }

  hid = kms_sdp_agent_append_media_handler (agent,
      gst_sdp_media_get_proto (media), handler);

  if (hid < 0) {
    g_object_unref (handler);
    return NULL;
  } else {
    return kms_sdp_agent_get_handler (agent, hid);
  }
}

static SdpHandler *
kms_sdp_agent_get_handler_for_media (KmsSdpAgent * agent,
    const GstSDPMedia * media)
{
  GSList *l;

  for (l = agent->priv->handlers; l != NULL; l = l->next) {
    SdpHandler *sdp_handler;

    sdp_handler = l->data;

    if (sdp_handler->sdph->negotiated) {
      continue;
    }

    if (g_strcmp0 (sdp_handler->sdph->media,
            gst_sdp_media_get_media (media)) != 0) {
      /* This handler can not manage this media */
      continue;
    }

    if (!kms_sdp_media_handler_manage_protocol (sdp_handler->sdph->handler,
            gst_sdp_media_get_proto (media))) {
      continue;
    }

    if (g_slist_find (agent->priv->offer_handlers, sdp_handler)) {
      /* Handler used for answering other media */
      continue;
    }

    return sdp_handler;
  }

  return kms_sdp_agent_request_handler (agent, media);
}

static SdpHandler *
kms_sdp_agent_create_reject_media_handler (KmsSdpAgent * agent,
    const GstSDPMedia * media)
{
  return kms_sdp_agent_create_media_handler (agent,
      gst_sdp_media_get_media (media), create_reject_handler ());
}

static SdpHandler *
kms_sdp_agent_replace_offered_handler (KmsSdpAgent * agent,
    const GstSDPMedia * media, SdpHandler * handler)
{
  SdpHandler *candidate;
  GSList *l;

  l = g_slist_find (agent->priv->offer_handlers, handler);
  if (l == NULL) {
    GST_ERROR_OBJECT (agent, "Can not get a new candidate");
    return NULL;
  }

  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (l->data));

  /* Try to get a new handler for this media */
  candidate = kms_sdp_agent_get_handler_for_media (agent, media);

  if (candidate == NULL) {
    GST_WARNING_OBJECT (agent,
        "No handler for '%s' media proto '%s' found",
        gst_sdp_media_get_media (media), gst_sdp_media_get_proto (media));
    candidate = kms_sdp_agent_create_reject_media_handler (agent, media);
  } else {
    candidate =
        (SdpHandler *) kms_ref_struct_ref (KMS_REF_STRUCT_CAST (candidate));
  }

  /* Upate position of the offer list with the new handler */
  l->data = candidate;

  return candidate;
}

static SdpHandler *
kms_sdp_agent_get_proper_handler (KmsSdpAgent * agent,
    const GstSDPMedia * media, SdpHandler * handler)
{
  if (!handler->rejected) {
    return handler;
  }

  if (gst_sdp_media_get_port (media) == 0) {
    /* Handler was rejected but media is still disabled */
    return handler;
  }

  /* Media was rejected and the new media is not desabled, this mean that */
  /* current slot has been replaced for another new media. Let's look for */
  /* a compatible handler to manage it                                    */

  handler->sdph->negotiated = FALSE;
  handler->rejected = FALSE;
  handler->offer = FALSE;

  if (g_strcmp0 (handler->sdph->media, gst_sdp_media_get_media (media)) != 0) {
    /* This handler can not manage this media */
    return kms_sdp_agent_replace_offered_handler (agent, media, handler);
  }

  if (kms_sdp_media_handler_manage_protocol (handler->sdph->handler,
          gst_sdp_media_get_proto (media))) {
    /* The previously rejected handler is capable of managing this media */
    return handler;
  } else {
    return kms_sdp_agent_replace_offered_handler (agent, media, handler);
  }
}

static SdpHandler *
kms_sdp_agent_select_handler_to_answer_media (KmsSdpAgent * agent, guint index,
    const GstSDPMedia * media, const GstSDPMessage * offer)
{
  SdpHandler *handler;

  if (agent->priv->offer_handlers == NULL) {
    return kms_sdp_agent_get_handler_for_media (agent, media);
  }

  handler = g_slist_nth_data (agent->priv->offer_handlers, index);

  if (handler != NULL) {
    return kms_sdp_agent_get_proper_handler (agent, media, handler);
  }

  if (handler == NULL) {
    /* the position is off the end of the list */
    handler = kms_sdp_agent_get_handler_for_media (agent, media);
  }

  return handler;
}

static gboolean
create_media_answer (const GstSDPMedia * media, struct SdpAnswerData *data)
{
  KmsSdpAgent *agent = data->agent;
  GstSDPMedia *answer_media = NULL, *offer_media;
  SdpMediaConfig *mconf = NULL;
  SdpHandler *sdp_handler;
  GError **err = data->err;
  gboolean ret = TRUE;

  sdp_handler = kms_sdp_agent_select_handler_to_answer_media (agent,
      data->index, media, data->offer);

  if (sdp_handler == NULL) {
    GST_WARNING_OBJECT (agent,
        "No handler for '%s' media proto '%s' found",
        gst_sdp_media_get_media (media), gst_sdp_media_get_proto (media));
    sdp_handler = kms_sdp_agent_create_reject_media_handler (agent, media);
  } else if (gst_sdp_media_get_port (media) == 0) {
    if (sdp_handler->sdph->negotiated) {
      answer_media = kms_sdp_agent_get_negotiated_media (agent, sdp_handler,
          err);
      if (answer_media == NULL) {
        return FALSE;
      }
    } else {
      /* Process offer as usual and reject it later */
      GST_WARNING_OBJECT (agent,
          "Not negotiated media offered with port set to 0");
      answer_media =
          kms_sdp_media_handler_create_answer (sdp_handler->sdph->handler,
          data->ctx, media, err);
    }

    /* RFC rfc3264 [8.2]: A stream that is offered with a port */
    /* of zero MUST be marked with port zero in the answer     */
    reject_sdp_media (&answer_media);
    goto answer;
  }

  answer_media =
      kms_sdp_media_handler_create_answer (sdp_handler->sdph->handler,
      data->ctx, media, err);

  if (answer_media == NULL) {
    ret = FALSE;
    goto end;
  }

  if (sdp_handler->unsupported) {
    goto answer;
  }

  offer_media = kms_sdp_media_handler_create_offer (sdp_handler->sdph->handler,
      gst_sdp_media_get_media (media), data->err);

  if (offer_media == NULL) {
    ret = FALSE;
    goto end;
  }

  update_media_offered (sdp_handler, offer_media);
  gst_sdp_media_free (offer_media);

answer:
  if (sdp_handler->unsupported && sdp_handler->unsupported_media == NULL) {
    gst_sdp_media_copy (answer_media, &sdp_handler->unsupported_media);
  }

  mconf = kms_sdp_message_context_add_media (data->ctx, answer_media, err);
  if (mconf == NULL) {
    ret = FALSE;
  }

end:
  if (!ret && is_internal_handler (agent, sdp_handler)) {
    /* Remove internal handler on error */
    kms_ref_struct_unref (KMS_REF_STRUCT_CAST (sdp_handler));
  } else if (ret && !g_slist_find (agent->priv->offer_handlers, sdp_handler)) {
    if (!is_internal_handler (agent, sdp_handler)) {
      /* This is not an internal handler */
      sdp_handler =
          (SdpHandler *) kms_ref_struct_ref (KMS_REF_STRUCT_CAST (sdp_handler));
    }

    /* add handler to the sdp ordered list */
    sdp_handler->sdph->index = data->index;
    agent->priv->offer_handlers = g_slist_append (agent->priv->offer_handlers,
        sdp_handler);
  }

  if (mconf != NULL) {
    kms_sdp_agent_fire_on_answer_callback (data->agent,
        sdp_handler->sdph->handler, mconf);
  }

  /* Update index for next media */
  data->index++;

  return ret;
}

static gboolean
kms_sdp_agent_cancel_offer_impl (KmsSdpAgent * agent, GError ** error)
{
  gboolean ret;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_LOCAL_OFFER) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "Agent in state %s", SDP_AGENT_STATE (agent));
    ret = FALSE;
  } else {
    KmsSdpAgentState new_state;

    new_state = (agent->priv->prev_sdp != NULL) ?
        KMS_SDP_AGENT_STATE_NEGOTIATED : KMS_SDP_AGENT_STATE_UNNEGOTIATED;

    SDP_AGENT_NEW_STATE (agent, new_state);

    ret = TRUE;
  }

  SDP_AGENT_UNLOCK (agent);

  return ret;
}

static SdpMessageContext *
kms_sdp_agent_generate_answer_compat (KmsSdpAgent * agent,
    const GstSDPMessage * offer, GError ** error)
{
  struct SdpAnswerData data;
  SdpMessageContext *ctx;
  gboolean bundle;
  GstSDPOrigin o;

  kms_sdp_agent_origin_init (agent, &o, agent->priv->local.id,
      agent->priv->local.version);
  bundle = g_slist_length (agent->priv->groups) > 0;

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
  data.offer = offer;
  data.index = 0;

  if (sdp_utils_for_each_media (offer, (GstSDPMediaFunc) create_media_answer,
          &data)) {
    return ctx;
  }

error:
  kms_sdp_message_context_unref (ctx);

  return NULL;
}

static SdpMessageContext *
kms_sdp_agent_create_answer_impl (KmsSdpAgent * agent, GError ** error)
{
  SdpMessageContext *ctx = NULL;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_REMOTE_OFFER) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "Agent in state %s", SDP_AGENT_STATE (agent));
    SDP_AGENT_UNLOCK (agent);
    return NULL;
  }

  if (agent->priv->local.id == NULL || agent->priv->local.version == NULL) {
    generate_sdp_session_description (&agent->priv->local);
  }

  ctx = kms_sdp_agent_generate_answer_compat (agent,
      agent->priv->remote_description, error);

  SDP_AGENT_UNLOCK (agent);

  return ctx;
}

static void
kms_sdp_agent_fire_on_answered_callback (KmsSdpAgent * agent,
    SdpHandler * sdp_handler, SdpMediaConfig * mconf, gboolean local_offerer)
{
  if (agent->priv->callbacks.callbacks.on_media_answered != NULL) {
    agent->priv->callbacks.callbacks.on_media_answered (agent,
        sdp_handler->sdph->handler, mconf, local_offerer,
        agent->priv->callbacks.user_data);
  }
}

static gint
handler_media_config_cmp_func (SdpHandler * h, gconstpointer * mid_pointer)
{
  gint mid = GPOINTER_TO_INT (mid_pointer);

  return h->sdph->index - mid;
}

typedef struct _KmsSdpAgentProcessAnsweredMediaConfigData
{
  KmsSdpAgent *agent;
  gboolean local_offerer;
} KmsSdpAgentProcessAnsweredMediaConfigData;

static void
kms_sdp_agent_process_answered_media_config (SdpMediaConfig * mconf,
    KmsSdpAgentProcessAnsweredMediaConfigData * data)
{
  KmsSdpAgent *agent = data->agent;
  gboolean local_offerer = data->local_offerer;
  gint mid = kms_sdp_media_config_get_id (mconf);
  GSList *l;

  l = g_slist_find_custom (agent->priv->offer_handlers, GINT_TO_POINTER (mid),
      (GCompareFunc) handler_media_config_cmp_func);
  if (l == NULL) {
    GST_WARNING_OBJECT (agent, "SDP handler not found for media posistion '%u'",
        mid);
    return;
  }

  kms_sdp_agent_fire_on_answered_callback (agent, l->data, mconf,
      local_offerer);
}

static void
kms_sdp_agent_process_answered_context (KmsSdpAgent * agent,
    SdpMessageContext * ctx, gboolean local_offerer)
{
  KmsSdpAgentProcessAnsweredMediaConfigData data;

  data.agent = agent;
  data.local_offerer = local_offerer;
  g_slist_foreach (kms_sdp_message_context_get_medias (ctx),
      (GFunc) kms_sdp_agent_process_answered_media_config, &data);
}

static void
kms_sdp_agent_process_answered_description (KmsSdpAgent * agent,
    GstSDPMessage * desc, gboolean local_offerer)
{
  SdpMessageContext *ctx;
  GError *err = NULL;

  ctx = kms_sdp_message_context_new_from_sdp (desc, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (agent, "Error generating SDP message context (%s)",
        err->message);
    g_error_free (err);
    return;
  }

  kms_sdp_agent_process_answered_context (agent, ctx, local_offerer);

  kms_sdp_message_context_unref (ctx);
}

static gboolean
kms_sdp_agent_set_local_description_impl (KmsSdpAgent * agent,
    GstSDPMessage * description, GError ** error)
{
  KmsSdpAgentState new_state;
  const GstSDPOrigin *orig;
  GError *err = NULL;
  gboolean ret = FALSE;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_LOCAL_OFFER &&
      agent->priv->state != KMS_SDP_AGENT_STATE_REMOTE_OFFER) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
        "Agent in state %s", SDP_AGENT_STATE (agent));
    goto end;
  }

  orig = gst_sdp_message_get_origin (description);
  if (g_strcmp0 (orig->sess_id, agent->priv->local.id)) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Unexpected sdp session %s", orig->sess_id);
    goto end;
  }

  if (agent->priv->local_description != NULL) {
    kms_sdp_message_context_unref (agent->priv->local_description);
  }

  agent->priv->local_description =
      kms_sdp_message_context_new_from_sdp (description, error);
  if (agent->priv->local_description == NULL) {
    goto end;
  }

  if (agent->priv->state == KMS_SDP_AGENT_STATE_REMOTE_OFFER) {
    if (agent->priv->prev_sdp != NULL) {
      gst_sdp_message_free (agent->priv->prev_sdp);
    }

    agent->priv->prev_sdp =
        kms_sdp_message_context_pack (agent->priv->local_description, &err);

    if (agent->priv->prev_sdp == NULL) {
      GST_ERROR_OBJECT (agent, "%s", err->message);
      g_error_free (err);
      goto end;
    }
  }

  new_state = (agent->priv->state == KMS_SDP_AGENT_STATE_LOCAL_OFFER) ?
      KMS_SDP_AGENT_STATE_WAIT_NEGO : KMS_SDP_AGENT_STATE_NEGOTIATED;
  SDP_AGENT_NEW_STATE (agent, new_state);
  ret = TRUE;

  if (new_state == KMS_SDP_AGENT_STATE_NEGOTIATED) {
    kms_sdp_agent_process_answered_context (agent,
        agent->priv->local_description, FALSE);
  }

end:
  SDP_AGENT_UNLOCK (agent);

  return ret;
}

static void
update_rejected_medias (KmsSdpAgent * agent, const GstSDPMessage * desc)
{
  guint i, len;

  len = gst_sdp_message_medias_len (desc);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;
    SdpHandler *handler;

    media = gst_sdp_message_get_media (desc, i);
    handler = g_slist_nth_data (agent->priv->offer_handlers, i);

    if (handler == NULL) {
      GST_DEBUG_OBJECT (agent, "No handler for media at position %u", i);
      continue;
    }

    if (!handler->rejected && gst_sdp_media_get_port (media) == 0) {
      handler->rejected = TRUE;
    }
  }
}

static void
kms_sdp_agent_process_answer (KmsSdpAgent * agent)
{
  GError *err = NULL;
  guint i, len;

  len = gst_sdp_message_medias_len (agent->priv->prev_sdp);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;
    SdpHandler *handler;

    media = gst_sdp_message_get_media (agent->priv->prev_sdp, i);
    handler = g_slist_nth_data (agent->priv->offer_handlers, i);

    if (handler == NULL) {
      GST_ERROR_OBJECT (agent, "Can not process answer in handler %u", i);
      continue;
    }

    if (!kms_sdp_media_handler_process_answer (handler->sdph->handler, media,
            &err)) {
      GST_ERROR_OBJECT (agent, "Error processing answer: %s", err->message);
      g_clear_error (&err);
    }
  }
}

static gboolean
kms_sdp_agent_set_remote_description_impl (KmsSdpAgent * agent,
    GstSDPMessage * description, GError ** error)
{
  gboolean ret = TRUE;

  SDP_AGENT_LOCK (agent);

  switch (agent->priv->state) {
    case KMS_SDP_AGENT_STATE_WAIT_NEGO:{
      const GstSDPOrigin *orig;

      orig = gst_sdp_message_get_origin (description);

      if (agent->priv->remote.id == NULL && agent->priv->remote.version == NULL) {
        /* First answer received from remote side => establish the session */
        set_sdp_session_description (&agent->priv->remote, orig->sess_id,
            orig->sess_version);
      } else if (g_strcmp0 (agent->priv->remote.id, orig->sess_id) != 0 ||
          g_strcmp0 (agent->priv->remote.version, orig->sess_version) != 0) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
            "Invalid sdp session %s %s, expected %s %s", orig->sess_id,
            orig->sess_version, agent->priv->remote.id,
            agent->priv->remote.version);
        ret = FALSE;
      }

      if (agent->priv->prev_sdp != NULL) {
        gst_sdp_message_free (agent->priv->prev_sdp);
      }

      update_rejected_medias (agent, description);
      gst_sdp_message_copy (description, &agent->priv->prev_sdp);
      kms_sdp_agent_process_answer (agent);
      SDP_AGENT_NEW_STATE (agent, KMS_SDP_AGENT_STATE_NEGOTIATED);

      break;
    }
    case KMS_SDP_AGENT_STATE_UNNEGOTIATED:{
      const GstSDPOrigin *orig;

      orig = gst_sdp_message_get_origin (description);
      set_sdp_session_description (&agent->priv->remote, orig->sess_id,
          orig->sess_version);
      SDP_AGENT_NEW_STATE (agent, KMS_SDP_AGENT_STATE_REMOTE_OFFER);
      break;
    }
    case KMS_SDP_AGENT_STATE_NEGOTIATED:{
      const GstSDPOrigin *orig;
      guint64 v1, v2;

      orig = gst_sdp_message_get_origin (description);
      v1 = g_ascii_strtoull (agent->priv->remote.version, NULL, 10);
      v2 = g_ascii_strtoull (orig->sess_version, NULL, 10);

      if ((g_strcmp0 (agent->priv->remote.id, orig->sess_id) == 0) &&
          (v1 == v2 || (v1 + 1) == v2)) {
        update_rejected_medias (agent, description);
        g_free (agent->priv->remote.version);
        agent->priv->remote.version = g_strdup (orig->sess_version);
        SDP_AGENT_NEW_STATE (agent, KMS_SDP_AGENT_STATE_REMOTE_OFFER);
      } else {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
            "Invalid sdp session: %s %s, expected %s %s", orig->sess_id,
            orig->sess_version, agent->priv->remote.id,
            agent->priv->remote.version);
        ret = FALSE;
      }
      break;
    }
    default:
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_STATE,
          "Agent in state %s", SDP_AGENT_STATE (agent));
      ret = FALSE;
  }

  if (ret) {
    kms_sdp_agent_release_sdp (&agent->priv->remote_description);
    agent->priv->remote_description = description;

    if (agent->priv->state == KMS_SDP_AGENT_STATE_NEGOTIATED) {
      kms_sdp_agent_process_answered_description (agent, description, TRUE);
    }
  }

  SDP_AGENT_UNLOCK (agent);

  return ret;
}

gint
kms_sdp_agent_create_bundle_group_impl (KmsSdpAgent * agent)
{
  SdpHandlerGroup *group;
  guint id;

  SDP_AGENT_LOCK (agent);

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    SDP_AGENT_UNLOCK (agent);
    GST_WARNING_OBJECT (agent,
        "Can not manipulate media while negotiation is taking place");
    return -1;
  }

  id = agent->priv->gids++;
  group = sdp_handler_group_new (id, NULL);
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

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    SDP_AGENT_UNLOCK (agent);
    GST_WARNING_OBJECT (agent,
        "Can not manipulate media while negotiation is taking place");
    goto end;
  }

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

    if (h->sdph->id == hid) {
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

  if (agent->priv->state != KMS_SDP_AGENT_STATE_UNNEGOTIATED &&
      agent->priv->state != KMS_SDP_AGENT_STATE_NEGOTIATED) {
    SDP_AGENT_UNLOCK (agent);
    GST_WARNING_OBJECT (agent,
        "Can not manipulate media while negotiation is taking place");
    goto end;
  }

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
  klass->get_handler_index = kms_sdp_agent_get_handler_index_impl;
  klass->create_offer = kms_sdp_agent_create_offer_impl;
  klass->create_answer = kms_sdp_agent_create_answer_impl;
  klass->cancel_offer = kms_sdp_agent_cancel_offer_impl;
  klass->set_local_description = kms_sdp_agent_set_local_description_impl;
  klass->set_remote_description = kms_sdp_agent_set_remote_description_impl;
  klass->create_bundle_group = kms_sdp_agent_create_bundle_group_impl;
  klass->add_handler_to_group = kms_sdp_agent_add_handler_to_group_impl;
  klass->remove_handler_from_group =
      kms_sdp_agent_remove_handler_from_group_impl;
  g_type_class_add_private (klass, sizeof (KmsSdpAgentPrivate));
}

static void
kms_sdp_agent_init_callbacks (KmsSdpAgent * self)
{
  self->priv->callbacks.user_data = NULL;
  self->priv->callbacks.destroy = NULL;
  self->priv->callbacks.callbacks.on_media_answer = NULL;
  self->priv->callbacks.callbacks.on_media_offer = NULL;
  self->priv->callbacks.callbacks.on_handler_required = NULL;
}

static void
kms_sdp_agent_init (KmsSdpAgent * self)
{
  self->priv = KMS_SDP_AGENT_GET_PRIVATE (self);

  self->priv->group_manager = kms_sdp_group_manager_new ();

  g_rec_mutex_init (&self->priv->mutex);
  self->priv->state = KMS_SDP_AGENT_STATE_UNNEGOTIATED;

  kms_sdp_agent_init_callbacks (self);
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

GstSDPMessage *
kms_sdp_agent_create_offer (KmsSdpAgent * agent, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_offer (agent, error);
}

/* Deprecated: Use kms_sdp_agent_generate_answer instead */
SdpMessageContext *
kms_sdp_agent_create_answer (KmsSdpAgent * agent, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), NULL);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_answer (agent, error);
}

gboolean
kms_sdpagent_cancel_offer (KmsSdpAgent * agent, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->cancel_offer (agent, error);
}

gboolean
kms_sdp_agent_set_local_description (KmsSdpAgent * agent,
    GstSDPMessage * description, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->set_local_description (agent,
      description, error);
}

gboolean
kms_sdp_agent_set_remote_description (KmsSdpAgent * agent,
    GstSDPMessage * description, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), FALSE);

  return KMS_SDP_AGENT_GET_CLASS (agent)->set_remote_description (agent,
      description, error);
}

gint
kms_sdp_agent_create_bundle_group (KmsSdpAgent * agent)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), -1);

  return KMS_SDP_AGENT_GET_CLASS (agent)->create_bundle_group (agent);
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

void
kms_sdp_agent_set_callbacks (KmsSdpAgent * agent,
    KmsSdpAgentCallbacks * callbacks, gpointer user_data,
    GDestroyNotify destroy)
{
  GDestroyNotify notify;
  gpointer old_data;

  g_return_if_fail (KMS_IS_SDP_AGENT (agent));

  SDP_AGENT_LOCK (agent);

  notify = agent->priv->callbacks.destroy;
  old_data = agent->priv->callbacks.user_data;

  agent->priv->callbacks.destroy = destroy;
  agent->priv->callbacks.user_data = user_data;
  agent->priv->callbacks.callbacks.on_media_answer = callbacks->on_media_answer;
  agent->priv->callbacks.callbacks.on_media_answered =
      callbacks->on_media_answered;
  agent->priv->callbacks.callbacks.on_media_offer = callbacks->on_media_offer;
  agent->priv->callbacks.callbacks.on_handler_required =
      callbacks->on_handler_required;

  SDP_AGENT_UNLOCK (agent);

  if (notify != NULL && old_data != NULL) {
    notify (old_data);
  }
}

gint
kms_sdp_agent_create_group (KmsSdpAgent * agent, GType group_type,
    const char *optname1, ...)
{
  gboolean failed;
  gpointer obj;
  va_list ap;
  gint gid;

  va_start (ap, optname1);
  obj = g_object_new_valist (group_type, optname1, ap);
  va_end (ap);

  if (!KMS_IS_SDP_BASE_GROUP (obj)) {
    GST_WARNING_OBJECT (agent, "Trying to create an invalid group");
    g_object_unref (obj);
    return -1;
  }

  SDP_AGENT_LOCK (agent);

  gid = kms_sdp_group_manager_add_group (agent->priv->group_manager,
      KMS_SDP_BASE_GROUP (obj));

  failed = gid < 0;

  if (!failed) {
    /* Add group extension */
    agent->priv->extensions = g_slist_append (agent->priv->extensions,
        g_object_ref (obj));
    agent->priv->groups = g_slist_append (agent->priv->groups,
        sdp_handler_group_new (gid, g_object_ref (obj)));
  }

  SDP_AGENT_UNLOCK (agent);

  if (failed) {
    GST_WARNING_OBJECT (agent, "Can not create group");
    g_object_unref (obj);
    return -1;
  }

  return gid;
}

gboolean
kms_sdp_agent_group_add (KmsSdpAgent * agent, guint gid, guint hid)
{
  gboolean ret;

  SDP_AGENT_LOCK (agent);

  ret = kms_sdp_group_manager_add_handler_to_group (agent->priv->group_manager,
      gid, hid);

  SDP_AGENT_UNLOCK (agent);

  return ret;
}

gint
kms_sdp_agent_get_handler_index (KmsSdpAgent * agent, gint hid)
{
  g_return_val_if_fail (KMS_IS_SDP_AGENT (agent), -1);

  return KMS_SDP_AGENT_GET_CLASS (agent)->get_handler_index (agent, hid);
}
