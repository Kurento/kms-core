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
#include "kmssdpmediahandler.h"

#define OBJECT_NAME "sdpmediahandler"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_media_handler_debug_category);
#define GST_CAT_DEFAULT kms_sdp_media_handler_debug_category

#define parent_class kms_sdp_media_handler_parent_class

G_DEFINE_TYPE_WITH_CODE (KmsSdpMediaHandler, kms_sdp_media_handler,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_media_handler_debug_category, OBJECT_NAME,
        0, "debug category for sdp media_handler"));

/* Object properties */
enum
{
  PROP_0,
  PROP_PROTO,
  N_PROPERTIES
};

#define KMS_SDP_MEDIA_HANDLER_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (           \
    (obj),                                \
    KMS_TYPE_SDP_MEDIA_HANDLER,                   \
    KmsSdpMediaHandlerPrivate                    \
  )                                       \
)
struct _KmsSdpMediaHandlerPrivate
{
  gchar *proto;
};

static void
kms_sdp_media_handler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsSdpMediaHandler *self = KMS_SDP_MEDIA_HANDLER (object);

  switch (prop_id) {
    case PROP_PROTO:
      g_value_set_string (value, self->priv->proto);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
kms_sdp_media_handler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsSdpMediaHandler *self = KMS_SDP_MEDIA_HANDLER (object);

  switch (prop_id) {
    case PROP_PROTO:
      self->priv->proto = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
kms_sdp_media_handler_finalize (GObject * object)
{
  KmsSdpMediaHandler *self = KMS_SDP_MEDIA_HANDLER (object);

  GST_DEBUG ("Finalize");

  g_free (self->priv->proto);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstSDPMedia *
kms_sdp_media_handler_create_offer_impl (KmsSdpMediaHandler * handler,
    const gchar * media, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Not implemented");

  return NULL;
}

static GstSDPMedia *
kms_sdp_media_handler_create_answer_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Not implemented");

  return NULL;
}

static gboolean
kms_sdp_media_handler_can_insert_attribute_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, const GstSDPAttribute * attr,
    GstSDPMedia * media)
{
  guint i, len;

  /* Returns TRUE only if this attribute is not already in media */

  len = gst_sdp_media_attributes_len (media);

  for (i = 0; i < len; i++) {
    const GstSDPAttribute *a;

    a = gst_sdp_media_get_attribute (media, i);

    if (g_strcmp0 (attr->key, a->key) == 0 &&
        g_strcmp0 (attr->value, a->value) == 0) {
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
kms_sdp_media_handler_intersect_sdp_medias_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Not implemented");

  return FALSE;
}

static void
kms_sdp_media_handler_class_init (KmsSdpMediaHandlerClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = kms_sdp_media_handler_get_property;
  gobject_class->set_property = kms_sdp_media_handler_set_property;
  gobject_class->finalize = kms_sdp_media_handler_finalize;

  g_object_class_install_property (gobject_class, PROP_PROTO,
      g_param_spec_string ("proto", "Protocol",
          "Media protocol", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  klass->create_offer = kms_sdp_media_handler_create_offer_impl;
  klass->create_answer = kms_sdp_media_handler_create_answer_impl;

  klass->can_insert_attribute = kms_sdp_media_handler_can_insert_attribute_impl;
  klass->intersect_sdp_medias = kms_sdp_media_handler_intersect_sdp_medias_impl;

  g_type_class_add_private (klass, sizeof (KmsSdpMediaHandlerPrivate));
}

static void
kms_sdp_media_handler_init (KmsSdpMediaHandler * self)
{
  self->priv = KMS_SDP_MEDIA_HANDLER_GET_PRIVATE (self);
}

GstSDPMedia *
kms_sdp_media_handler_create_offer (KmsSdpMediaHandler * handler,
    const gchar * media, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_MEDIA_HANDLER (handler), NULL);

  return KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->create_offer (handler,
      media, error);
}

GstSDPMedia *
kms_sdp_media_handler_create_answer (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GError ** error)
{
  g_return_val_if_fail (KMS_IS_SDP_MEDIA_HANDLER (handler), NULL);

  return KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->create_answer (handler,
      offer, error);
}
