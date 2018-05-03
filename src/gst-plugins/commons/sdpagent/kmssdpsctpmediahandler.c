/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "kmssdpagent.h"
#include "sdp_utils.h"
#include "kmssdpsctpmediahandler.h"

#define OBJECT_NAME "sdpsctpmediahandler"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_sctp_media_handler_debug_category);
#define GST_CAT_DEFAULT kms_sdp_sctp_media_handler_debug_category

#define parent_class kms_sdp_sctp_media_handler_parent_class

G_DEFINE_TYPE_WITH_CODE (KmsSdpSctpMediaHandler, kms_sdp_sctp_media_handler,
    KMS_TYPE_SDP_MEDIA_HANDLER,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_sctp_media_handler_debug_category,
        OBJECT_NAME, 0, "debug category for sdp sctp media_handler"));

#define SDP_MEDIA_DTLS_SCTP_PROTO "DTLS/SCTP"
#define SDP_MEDIA_DTLS_SCTP_FMT "5000"

/* suggested value by draft-ietf-mmusic-sctp-sdp */
#define DEFAULT_STREAMS_N 16

static gchar *sctpsubprotos[] = {
  "webrtc-datachannel",
  /* Add more if needed */
};

static GObject *
kms_sdp_sctp_media_handler_constructor (GType gtype, guint n_properties,
    GObjectConstructParam * properties)
{
  GObjectConstructParam *property;
  gchar const *name;
  GObject *object;
  guint i;

  for (i = 0, property = properties; i < n_properties; ++i, ++property) {
    name = g_param_spec_get_name (property->pspec);
    if (g_strcmp0 (name, "proto") == 0) {
      /* change G_PARAM_CONSTRUCT_ONLY value */
      g_value_set_string (property->value, SDP_MEDIA_DTLS_SCTP_PROTO);
    }
  }

  object =
      G_OBJECT_CLASS (parent_class)->constructor (gtype, n_properties,
      properties);

  return object;
}

static GstSDPMedia *
kms_sdp_sctp_media_handler_create_offer (KmsSdpMediaHandler * handler,
    const gchar * media, const GstSDPMedia * prev_offer, GError ** error)
{
  GstSDPMedia *m = NULL;

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not to create '%s' media", media);
    goto error;
  }

  /* Create m-line */
  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->init_offer (handler, media, m,
          prev_offer, error)) {
    goto error;
  }

  /* Add attributes to m-line */
  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->add_offer_attributes (handler,
          m, prev_offer, error)) {
    goto error;
  }

  return m;

error:
  if (m != NULL) {
    gst_sdp_media_free (m);
  }

  return NULL;
}

static gboolean
subproto_supported (const gchar * subproto)
{
  guint i, len;

  len = G_N_ELEMENTS (sctpsubprotos);

  for (i = 0; i < len; i++) {
    if (g_strcmp0 (subproto, sctpsubprotos[i]) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
format_supported (const GstSDPMedia * media, const gchar * fmt)
{
  const gchar *val;
  gchar **attrs;
  gboolean ret;

  val = sdp_utils_get_attr_map_value (media, "sctpmap", fmt);

  if (val == NULL) {
    return FALSE;
  }

  attrs = g_strsplit (val, " ", 0);
  ret = subproto_supported (attrs[1] /* sub-protocol */ );
  g_strfreev (attrs);

  return ret;
}

static gboolean
add_supported_sctmap_attrs (const GstSDPMedia * offer, GstSDPMedia * answer,
    GError ** error)
{
  guint i, len;

  len = gst_sdp_media_formats_len (answer);

  for (i = 0; i < len; i++) {
    const gchar *fmt, *val;

    fmt = gst_sdp_media_get_format (answer, i);
    val = sdp_utils_get_attr_map_value (offer, "sctpmap", fmt);

    if (val == NULL) {
      GST_WARNING ("Not 'sctpmap:%s' attribute found in offer", fmt);
      continue;
    }

    if (gst_sdp_media_add_attribute (answer, "sctpmap", val) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not add attribute 'sctpmap:%s'", val);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
add_supported_subproto_attrs (const GstSDPMedia * offer, GstSDPMedia * answer,
    GError ** error)
{
  guint i, len;

  len = gst_sdp_media_formats_len (answer);

  for (i = 0; i < len; i++) {
    const gchar *fmt, *val;
    gchar **attrs;
    guint j;

    fmt = gst_sdp_media_get_format (answer, i);
    val = sdp_utils_get_attr_map_value (offer, "sctpmap", fmt);

    if (val == NULL) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Not 'sctpmap:%s' attribute found in offer", fmt);
      return FALSE;
    }

    attrs = g_strsplit (val, " ", 0);

    for (j = 0;; j++) {
      const gchar *attr;

      attr = gst_sdp_media_get_attribute_val_n (offer,
          attrs[1] /* sub-protocol */ , j);

      if (attr == NULL) {
        break;
      }

      if (gst_sdp_media_add_attribute (answer, attrs[1], attr) != GST_SDP_OK) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Can add attribute '%s:%s'", attrs[1], val);
        g_strfreev (attrs);
        return FALSE;
      }
    }

    g_strfreev (attrs);
  }

  return TRUE;
}

static gboolean
is_subproto (const GstSDPMedia * media, const gchar * label)
{
  guint i, len;

  len = gst_sdp_media_formats_len (media);

  for (i = 0; i < len; i++) {
    const gchar *fmt, *val;
    gchar **attrs;

    fmt = gst_sdp_media_get_format (media, i);
    val = sdp_utils_get_attr_map_value (media, "sctpmap", fmt);

    if (val == NULL) {
      GST_WARNING ("Not 'sctpmap:%s' attribute found in offer", fmt);
      continue;
    }

    attrs = g_strsplit (val, " ", 0);

    if (g_strcmp0 (label, attrs[1] /* protocol */ ) == 0) {
      /* val is a subproto */
      g_strfreev (attrs);
      return TRUE;
    }

    g_strfreev (attrs);
  }

  return FALSE;
}

static gboolean
kms_sdp_sctp_media_handler_can_insert_attribute (KmsSdpMediaHandler *
    handler, const GstSDPMedia * offer, const GstSDPAttribute * attr,
    GstSDPMedia * answer, const GstSDPMessage * msg)
{
  if (g_strcmp0 (attr->key, "sctpmap") == 0 || is_subproto (offer, attr->key)) {
    /* ignore */
    return FALSE;
  }

  if (sdp_utils_attribute_is_direction (attr, NULL)) {
    /* SDP direction attributes MUST be discarded if present. */
    /* [draft-ietf-mmusic-sctp-sdp] 9.2                       */
    return FALSE;
  }

  if (!KMS_SDP_MEDIA_HANDLER_CLASS (parent_class)->can_insert_attribute
      (handler, offer, attr, answer, msg)) {
    return FALSE;
  }

  return TRUE;
}

GstSDPMedia *
kms_sdp_sctp_media_handler_create_answer (KmsSdpMediaHandler * handler,
    const GstSDPMessage * msg, const GstSDPMedia * offer, GError ** error)
{
  GstSDPMedia *m = NULL;

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not create '%s' media answer", gst_sdp_media_get_media (offer));
    goto error;
  }

  /* Create m-line */
  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->init_answer (handler, offer,
          m, error)) {
    goto error;
  }

  /* Add attributes to m-line */
  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->add_answer_attributes
      (handler, offer, m, error)) {
    goto error;
  }

  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (handler)->intersect_sdp_medias (handler,
          offer, m, msg, error)) {
    goto error;
  }

  return m;

error:

  if (m != NULL) {
    gst_sdp_media_free (m);
  }

  return NULL;
}

struct intersect_data
{
  KmsSdpMediaHandler *handler;
  const GstSDPMedia *offer;
  GstSDPMedia *answer;
  const GstSDPMessage *msg;
};

static gboolean
instersect_sctp_media_attr (const GstSDPAttribute * attr, gpointer user_data)
{
  struct intersect_data *data = (struct intersect_data *) user_data;

  if (!KMS_SDP_MEDIA_HANDLER_GET_CLASS (data->
          handler)->can_insert_attribute (data->handler, data->offer, attr,
          data->answer, data->msg)) {
    return FALSE;
  }

  if (gst_sdp_media_add_attribute (data->answer, attr->key,
          attr->value) != GST_SDP_OK) {
    GST_WARNING ("Can not add attribute '%s'", attr->key);
    return FALSE;
  }

  return TRUE;
}

static gboolean
kms_sdp_sctp_media_handler_intersect_sdp_medias (KmsSdpMediaHandler *
    handler, const GstSDPMedia * offer, GstSDPMedia * answer,
    const GstSDPMessage * msg, GError ** error)
{
  struct intersect_data data = {
    .handler = handler,
    .offer = offer,
    .answer = answer,
    .msg = msg
  };

  if (!sdp_utils_intersect_media_attributes (offer,
          instersect_sctp_media_attr, &data)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_UNEXPECTED_ERROR, "Can not intersect media attributes");
    return FALSE;
  }

  return TRUE;
}

static gboolean
kms_sdp_sctp_media_handler_init_offer (KmsSdpMediaHandler * handler,
    const gchar * media, GstSDPMedia * offer, const GstSDPMedia * prev_offer,
    GError ** error)
{
  gboolean ret = TRUE;

  if (g_strcmp0 (media, "application") != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported '%s' media", media);
    ret = FALSE;
    goto end;
  }

  if (gst_sdp_media_set_media (offer, media) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not to set '%s' media", media);
    ret = FALSE;
    goto end;
  }

  if (gst_sdp_media_set_proto (offer, SDP_MEDIA_DTLS_SCTP_PROTO) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not to set '%s' protocol", SDP_MEDIA_DTLS_SCTP_PROTO);
    ret = FALSE;
    goto end;
  }

  if (gst_sdp_media_set_port_info (offer, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not to set port");
    ret = FALSE;
    goto end;
  }

  if (gst_sdp_media_add_format (offer, SDP_MEDIA_DTLS_SCTP_FMT) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not to set format");
    ret = FALSE;
    goto end;
  }

  if (gst_sdp_media_add_attribute (offer, "setup", "actpass") != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not set attribute 'setup:actpass'");
    ret = FALSE;
    goto end;
  }

end:
  return ret;
}

static gboolean
kms_sdp_sctp_media_handler_add_offer_attributes (KmsSdpMediaHandler * handler,
    GstSDPMedia * offer, const GstSDPMedia * prev_offer, GError ** error)
{
  guint i, len;

  /* Format parameters when protocol is DTLS/SCTP carries the SCTP port  */
  /* number and the mandatory "a=sctpmap:" attribute contains the actual */
  /* media format within the protocol parameter.                         */
  /* draft: https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-03    */
  len = gst_sdp_media_formats_len (offer);
  for (i = 0; i < len; i++) {
    const gchar *sctp_port;
    gchar *attr;

    sctp_port = gst_sdp_media_get_format (offer, i);
    attr = g_strdup_printf ("%s webrtc-datachannel %d",
        sctp_port, DEFAULT_STREAMS_N);

    if (gst_sdp_media_add_attribute (offer, "sctpmap", attr) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not to set attribute 'sctpman:%s'", attr);
      g_free (attr);
      return FALSE;
    }

    g_free (attr);
  }

  /* Chain up */
  return
      KMS_SDP_MEDIA_HANDLER_CLASS (parent_class)->add_offer_attributes (handler,
      offer, prev_offer, error);
}

static gboolean
kms_sdp_sctp_media_handler_init_answer (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  gchar *proto = NULL;

  if (g_strcmp0 (gst_sdp_media_get_media (offer), "application") != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported '%s' media", gst_sdp_media_get_media (offer));
    goto error;
  }

  g_object_get (handler, "proto", &proto, NULL);

  if (g_strcmp0 (proto, gst_sdp_media_get_proto (offer)) != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PROTOCOL,
        "Unexpected media protocol '%s'", gst_sdp_media_get_proto (offer));
    goto error;
  }

  if (gst_sdp_media_set_media (answer,
          gst_sdp_media_get_media (offer)) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set '%s' media ttribute", gst_sdp_media_get_media (offer));
    goto error;
  }

  if (gst_sdp_media_set_proto (answer, proto) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set proto '%s' attribute", proto);
    goto error;
  }

  if (gst_sdp_media_set_port_info (answer, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set port attribute");
    goto error;
  }

  g_free (proto);

  return TRUE;

error:
  if (proto != NULL) {
    g_free (proto);
  }

  return FALSE;
}

static gboolean
kms_sdp_sctp_media_handler_add_answer_attributes_impl (KmsSdpMediaHandler *
    handler, const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  guint i, len;

  if (!KMS_SDP_MEDIA_HANDLER_CLASS (parent_class)->add_answer_attributes
      (handler, offer, answer, error)) {
    return FALSE;
  }

  len = gst_sdp_media_formats_len (offer);

  /* Set only supported media formats in answer */
  for (i = 0; i < len; i++) {
    const gchar *fmt;

    fmt = gst_sdp_media_get_format (offer, i);

    if (!format_supported (offer, fmt)) {
      continue;
    }

    if (gst_sdp_media_add_format (answer, fmt) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not add format '%s'", fmt);
      return FALSE;
    }
  }

  if (!add_supported_sctmap_attrs (offer, answer, error)) {
    return FALSE;
  }

  return add_supported_subproto_attrs (offer, answer, error);
}

static void
kms_sdp_sctp_media_handler_class_init (KmsSdpSctpMediaHandlerClass * klass)
{
  GObjectClass *gobject_class;
  KmsSdpMediaHandlerClass *handler_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = kms_sdp_sctp_media_handler_constructor;

  handler_class = KMS_SDP_MEDIA_HANDLER_CLASS (klass);

  handler_class->create_offer = kms_sdp_sctp_media_handler_create_offer;
  handler_class->create_answer = kms_sdp_sctp_media_handler_create_answer;

  handler_class->can_insert_attribute =
      kms_sdp_sctp_media_handler_can_insert_attribute;
  handler_class->intersect_sdp_medias =
      kms_sdp_sctp_media_handler_intersect_sdp_medias;

  handler_class->init_offer = kms_sdp_sctp_media_handler_init_offer;
  handler_class->add_offer_attributes =
      kms_sdp_sctp_media_handler_add_offer_attributes;

  handler_class->init_answer = kms_sdp_sctp_media_handler_init_answer;
  handler_class->add_answer_attributes =
      kms_sdp_sctp_media_handler_add_answer_attributes_impl;
}

static void
kms_sdp_sctp_media_handler_init (KmsSdpSctpMediaHandler * self)
{
  /* Nothing to do here */
}

KmsSdpSctpMediaHandler *
kms_sdp_sctp_media_handler_new ()
{
  KmsSdpSctpMediaHandler *handler;

  handler =
      KMS_SDP_SCTP_MEDIA_HANDLER (g_object_new (KMS_TYPE_SDP_SCTP_MEDIA_HANDLER,
          NULL));

  return handler;
}
