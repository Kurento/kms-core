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

#include "sdp_utils.h"
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

#define DEFAULT_ADDR_TYPE "IP4"

typedef gboolean (*KmsSdpAcceptAttributeFunc) (const GstSDPMedia * offer,
    const GstSDPAttribute * attr, GstSDPMedia * media);

static gboolean
default_accept_attribute (const GstSDPMedia * offer,
    const GstSDPAttribute * attr, GstSDPMedia * media)
{
  return TRUE;
}

static gboolean
accept_fmtp_attribute (const GstSDPMedia * offer,
    const GstSDPAttribute * attr, GstSDPMedia * media)
{
  guint i, len;
  gchar **fmtp;
  gboolean ret = FALSE;

  fmtp = g_strsplit (attr->value, " ", 0);

  /* Check that answer supports this format */
  len = gst_sdp_media_formats_len (media);

  for (i = 0; i < len; i++) {
    const gchar *fmt;

    fmt = gst_sdp_media_get_format (media, i);
    if (g_strcmp0 (fmt, fmtp[0] /* format */ ) == 0) {
      ret = TRUE;
      break;
    }
  }

  g_strfreev (fmtp);

  return ret;
}

typedef struct _KmsSdpSupportedAttrType
{
  const gchar *name;
  KmsSdpAcceptAttributeFunc accept;
} KmsSdpSupportedAttrType;

/* Supported media session attributes */
static KmsSdpSupportedAttrType attributes[] = {
  {"framerate", default_accept_attribute},
  {"fmtp", accept_fmtp_attribute},
  {"lang", default_accept_attribute},
  {"maxptime", default_accept_attribute},
  {"mid", default_accept_attribute},
  {"ptime", default_accept_attribute},
  {"quality", default_accept_attribute},
  {"setup", default_accept_attribute}
};

/* Object properties */
enum
{
  PROP_0,
  PROP_PROTO,
  PROP_ADDR,
  PROP_ADDR_TYPE,
  N_PROPERTIES
};

#define KMS_SDP_MEDIA_HANDLER_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                   \
    (obj),                                        \
    KMS_TYPE_SDP_MEDIA_HANDLER,                   \
    KmsSdpMediaHandlerPrivate                     \
  )                                               \
)

struct _KmsSdpMediaHandlerPrivate
{
  gchar *proto;
  gchar *addr;
  gchar *addr_type;
  GArray *bwtypes;
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
    case PROP_ADDR:
      g_value_set_string (value, self->priv->addr);
      break;
    case PROP_ADDR_TYPE:
      g_value_set_string (value, self->priv->addr_type);
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
      g_free (self->priv->proto);
      self->priv->proto = g_value_dup_string (value);
      break;
    case PROP_ADDR:
      g_free (self->priv->addr);
      self->priv->addr = g_value_dup_string (value);
      break;
    case PROP_ADDR_TYPE:
      g_free (self->priv->addr_type);
      self->priv->addr_type = g_value_dup_string (value);
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

  GST_DEBUG_OBJECT (self, "finalize");

  g_free (self->priv->proto);
  g_free (self->priv->addr);
  g_free (self->priv->addr_type);

  g_array_free (self->priv->bwtypes, TRUE);

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

static void
kms_sdp_media_handler_add_bandwidth_impl (KmsSdpMediaHandler * handler,
    const gchar * bwtype, guint bandwidth)
{
  GstSDPBandwidth bw;

  gst_sdp_bandwidth_set (&bw, bwtype, bandwidth);
  g_array_append_val (handler->priv->bwtypes, bw);
}

static gboolean
kms_sdp_media_handler_can_insert_attribute_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, const GstSDPAttribute * attr,
    GstSDPMedia * media)
{
  guint i, len;

  if (sdp_utils_is_attribute_in_media (media, attr)) {
    return FALSE;
  }

  if (sdp_utils_attribute_is_direction (attr, NULL)) {
    return TRUE;
  }

  len = G_N_ELEMENTS (attributes);

  for (i = 0; i < len; i++) {
    if (g_strcmp0 (attr->key, attributes[i].name) == 0) {
      return attributes[i].accept (offer, attr, media);
    }
  }

  return FALSE;
}

static gboolean
kms_sdp_media_handler_intersect_sdp_medias_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Not implemented");

  return FALSE;
}

static gboolean
kms_sdp_media_handler_init_offer_impl (KmsSdpMediaHandler * handler,
    const gchar * media, GstSDPMedia * offer, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Media offert initialization is not implemented");

  return FALSE;
}

static gboolean
kms_sdp_media_handler_add_offer_attributes_impl (KmsSdpMediaHandler * handler,
    GstSDPMedia * offer, GError ** error)
{
  gint i;

  /* Add bandwidth attributes */
  for (i = 0; i < handler->priv->bwtypes->len; i++) {
    GstSDPBandwidth *bw;

    bw = &g_array_index (handler->priv->bwtypes, GstSDPBandwidth, i);
    gst_sdp_media_add_bandwidth (offer, bw->bwtype, bw->bandwidth);
  }

  return TRUE;
}

static gboolean
kms_sdp_media_handler_init_answer_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
      "Media answer initialization is not implemented");

  return FALSE;
}

static gboolean
kms_sdp_media_handler_add_answer_attributes_impl (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  guint i, len;

  /* Add bandwidth attributes */
  len = gst_sdp_media_bandwidths_len (offer);

  for (i = 0; i < len; i++) {
    const GstSDPBandwidth *offered_bw;
    guint j;

    offered_bw = gst_sdp_media_get_bandwidth (offer, i);

    /* look up an appropriate bandwidth attribute to intersect this one */
    for (j = 0; j < handler->priv->bwtypes->len; j++) {
      GstSDPBandwidth *bw;
      guint val;

      bw = &g_array_index (handler->priv->bwtypes, GstSDPBandwidth, j);

      if (g_strcmp0 (offered_bw->bwtype, bw->bwtype) != 0) {
        continue;
      }

      val = (offered_bw->bandwidth < bw->bandwidth) ? offered_bw->bandwidth :
          bw->bandwidth;

      gst_sdp_media_add_bandwidth (answer, bw->bwtype, val);
    }
  }

  return TRUE;
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

  g_object_class_install_property (gobject_class, PROP_ADDR,
      g_param_spec_string ("addr", "Address", "Address", NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ADDR_TYPE,
      g_param_spec_string ("addr-type",
          "Address type", "Address type either IP4 or IP6", DEFAULT_ADDR_TYPE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  klass->create_offer = kms_sdp_media_handler_create_offer_impl;
  klass->create_answer = kms_sdp_media_handler_create_answer_impl;
  klass->add_bandwidth = kms_sdp_media_handler_add_bandwidth_impl;

  klass->can_insert_attribute = kms_sdp_media_handler_can_insert_attribute_impl;
  klass->intersect_sdp_medias = kms_sdp_media_handler_intersect_sdp_medias_impl;

  klass->init_offer = kms_sdp_media_handler_init_offer_impl;
  klass->add_offer_attributes = kms_sdp_media_handler_add_offer_attributes_impl;

  klass->init_answer = kms_sdp_media_handler_init_answer_impl;
  klass->add_answer_attributes =
      kms_sdp_media_handler_add_answer_attributes_impl;

  g_type_class_add_private (klass, sizeof (KmsSdpMediaHandlerPrivate));
}

static void
kms_sdp_media_handler_init (KmsSdpMediaHandler * self)
{
  self->priv = KMS_SDP_MEDIA_HANDLER_GET_PRIVATE (self);
  self->priv->bwtypes = g_array_new (FALSE, TRUE, sizeof (GstSDPBandwidth));
  g_array_set_clear_func (self->priv->bwtypes,
      (GDestroyNotify) gst_sdp_bandwidth_clear);
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

void
kms_sdp_media_handler_add_bandwidth (KmsSdpMediaHandler * handler,
    const gchar * bwtype, guint bandwidth)
{
  g_return_val_if_fail (KMS_IS_SDP_MEDIA_HANDLER (handler), FALSE);

  return KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->add_bandwidth (handler,
      bwtype, bandwidth);
}
