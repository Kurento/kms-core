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

#include "kmssdpbasegroup.h"

#define OBJECT_NAME "sdpbasegroup"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_base_group_debug_category);
#define GST_CAT_DEFAULT kms_sdp_base_group_debug_category

#define parent_class kms_sdp_base_group_parent_class

static void kms_i_sdp_session_extension_init (KmsISdpSessionExtensionInterface *
    iface);

G_DEFINE_TYPE_WITH_CODE (KmsSdpBaseGroup, kms_sdp_base_group,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_SDP_SESSION_EXTENSION,
        kms_i_sdp_session_extension_init)
    GST_DEBUG_CATEGORY_INIT (kms_sdp_base_group_debug_category, OBJECT_NAME,
        0, "debug category for sdp base_group"));

#define KMS_SDP_BASE_GROUP_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                \
    (obj),                                     \
    KMS_TYPE_SDP_BASE_GROUP,                   \
    KmsSdpBaseGroupPrivate                     \
  )                                            \
)

struct _KmsSdpBaseGroupPrivate
{
  gchar *semantics;
  gboolean pre_proc;
  GSList *handlers;
};

/* Object properties */
enum
{
  PROP_0,
  PROP_SEMANTICS,
  PROP_PRE_PROC,
  N_PROPERTIES
};

static void
kms_sdp_base_group_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (object);

  switch (prop_id) {
    case PROP_SEMANTICS:
      g_value_set_string (value, self->priv->semantics);
      break;
    case PROP_PRE_PROC:
      g_value_set_boolean (value, self->priv->pre_proc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
kms_sdp_base_group_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (object);

  switch (prop_id) {
    case PROP_SEMANTICS:
      g_free (self->priv->semantics);
      self->priv->semantics = g_value_dup_string (value);
      break;
    case PROP_PRE_PROC:
      self->priv->pre_proc = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
kms_sdp_base_group_finalize (GObject * object)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_slist_free_full (self->priv->handlers,
      (GDestroyNotify) kms_sdp_agent_common_unref_sdp_handler);

  g_free (self->priv->semantics);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

typedef struct _SdpGroupStrVal
{
  gchar *str;
  GstSDPMessage *msg;
  KmsSdpBaseGroup *group;
} SdpGroupStrVal;

static void
append_enabled_medias (KmsSdpHandler * handler, SdpGroupStrVal * val)
{
  const GstSDPMedia *media;
  const gchar *mid;
  gchar *tmp;

  if (handler->index > gst_sdp_message_medias_len (val->msg)) {
    GST_ERROR_OBJECT (val->group, "Index %u for handler (%u) out of SDP range",
        handler->index, handler->id);
    return;
  }

  media = gst_sdp_message_get_media (val->msg, handler->index);

  if (media == NULL) {
    GST_ERROR_OBJECT (val->group, "No media got in SDP");
    return;
  }

  mid = gst_sdp_media_get_attribute_val (media, "mid");

  if (mid == NULL) {
    GST_WARNING_OBJECT (val->group,
        "No mid attribute for media %u. Skipping from group", handler->index);
    return;
  }

  tmp = val->str;
  val->str = g_strdup_printf ("%s %s", tmp, mid);

  g_free (tmp);
}

static gboolean
kms_sdp_base_group_add_offer_attributes_impl (KmsSdpBaseGroup * self,
    GstSDPMessage * offer, GError ** error)
{
  SdpGroupStrVal val;
  gboolean pre_proc;

  g_object_get (self, "pre-media-processing", &pre_proc, NULL);

  if (pre_proc) {
    /* Let chlidren classes manage this case */
    return TRUE;
  }

  val.msg = offer;
  val.group = self;
  val.str = g_strdup (self->priv->semantics);

  /* Add all handlers that are not disabled to this group */
  g_slist_foreach (self->priv->handlers, (GFunc) append_enabled_medias, &val);

  gst_sdp_message_add_attribute (offer, "group", val.str);

  g_free (val.str);

  return TRUE;
}

static gboolean
kms_sdp_base_group_add_answer_attributes_impl (KmsSdpBaseGroup * self,
    const GstSDPMessage * offer, GstSDPMessage * answer, GError ** error)
{
  /* Nothing to add at this level */
  return TRUE;
}

static gboolean
kms_sdp_base_group_can_insert_attribute_impl (KmsSdpBaseGroup * self,
    const GstSDPMessage * offer, const GstSDPAttribute * attr,
    GstSDPMessage * answer)
{
  /* Nothing to add at this level */
  return FALSE;
}

static gboolean
kms_sdp_base_group_add_media_handler_impl (KmsSdpBaseGroup * grp,
    KmsSdpHandler * handler, GError ** error)
{
  /* Allows to add every handler */
  grp->priv->handlers = g_slist_append (grp->priv->handlers,
      kms_sdp_agent_common_ref_sdp_handler (handler));

  /* TODO: Add this group to handler->groups in new API */

  return TRUE;
}

static gboolean
kms_sdp_base_group_remove_media_handler_impl (KmsSdpBaseGroup * grp,
    KmsSdpHandler * handler, GError ** error)
{
  if (g_slist_find (grp->priv->handlers, handler) == NULL) {
    g_set_error (error, KMS_SDP_BASE_GROUP_ERROR,
        SDP_BASE_GROUP_HANDLER_NOT_FOUND,
        "Media handler with id '%u' not found", handler->id);
    return FALSE;
  }

  grp->priv->handlers = g_slist_remove (grp->priv->handlers, handler);
  kms_sdp_agent_common_unref_sdp_handler (handler);

  /* TODO: Remove this group from handler->groups in new API */

  return TRUE;
}

static void
kms_sdp_base_group_class_init (KmsSdpBaseGroupClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = kms_sdp_base_group_get_property;
  gobject_class->set_property = kms_sdp_base_group_set_property;
  gobject_class->finalize = kms_sdp_base_group_finalize;

  g_object_class_install_property (gobject_class, PROP_SEMANTICS,
      g_param_spec_string ("semantics", "Semantics",
          "Semantics of this group", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_override_property (gobject_class, PROP_PRE_PROC,
      "pre-media-processing");

  klass->add_offer_attributes = kms_sdp_base_group_add_offer_attributes_impl;
  klass->add_answer_attributes = kms_sdp_base_group_add_answer_attributes_impl;
  klass->can_insert_attribute = kms_sdp_base_group_can_insert_attribute_impl;

  klass->add_media_handler = kms_sdp_base_group_add_media_handler_impl;
  klass->remove_media_handler = kms_sdp_base_group_remove_media_handler_impl;

  g_type_class_add_private (klass, sizeof (KmsSdpBaseGroupPrivate));
}

static void
kms_sdp_base_group_init (KmsSdpBaseGroup * self)
{
  self->priv = KMS_SDP_BASE_GROUP_GET_PRIVATE (self);
}

static gboolean
kms_sdp_base_group_add_offer_attributes (KmsISdpSessionExtension * ext,
    GstSDPMessage * offer, GError ** error)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (ext);

  return KMS_SDP_BASE_GROUP_GET_CLASS (self)->add_offer_attributes (self, offer,
      error);
}

static gboolean
kms_sdp_base_group_add_answer_attributes (KmsISdpSessionExtension * ext,
    const GstSDPMessage * offer, GstSDPMessage * answer, GError ** error)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (ext);

  return KMS_SDP_BASE_GROUP_GET_CLASS (self)->add_answer_attributes (self,
      offer, answer, error);
}

static gboolean
kms_sdp_base_group_can_insert_attribute (KmsISdpSessionExtension * ext,
    const GstSDPMessage * offer, const GstSDPAttribute * attr,
    GstSDPMessage * answer)
{
  KmsSdpBaseGroup *self = KMS_SDP_BASE_GROUP (ext);

  return KMS_SDP_BASE_GROUP_GET_CLASS (self)->can_insert_attribute (self, offer,
      attr, answer);
}

void
kms_i_sdp_session_extension_init (KmsISdpSessionExtensionInterface * iface)
{
  iface->add_offer_attributes = kms_sdp_base_group_add_offer_attributes;
  iface->add_answer_attributes = kms_sdp_base_group_add_answer_attributes;
  iface->can_insert_attribute = kms_sdp_base_group_can_insert_attribute;
}

gboolean
kms_sdp_base_group_add_media_handler (KmsSdpBaseGroup * grp,
    KmsSdpHandler * handler, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_BASE_GROUP (grp), FALSE);

  return KMS_SDP_BASE_GROUP_GET_CLASS (grp)->add_media_handler (grp, handler,
      error);
}

gboolean
kms_sdp_base_group_remove_media_handler (KmsSdpBaseGroup * grp,
    KmsSdpHandler * handler, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_BASE_GROUP (grp), FALSE);

  return KMS_SDP_BASE_GROUP_GET_CLASS (grp)->remove_media_handler (grp, handler,
      error);
}
