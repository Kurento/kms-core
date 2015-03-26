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
    const gchar * media, GError ** error)
{
  GstSDPMedia *m = NULL;
  guint i;

  if (g_strcmp0 (media, "application") != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported %s media", media);
    goto error;
  }

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to create %s media", media);
    goto error;
  }

  if (gst_sdp_media_set_media (m, media) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set %s media", media);
    goto error;
  }

  if (gst_sdp_media_set_proto (m, SDP_MEDIA_DTLS_SCTP_PROTO) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set %s protocol", SDP_MEDIA_DTLS_SCTP_PROTO);
    goto error;
  }

  if (gst_sdp_media_set_port_info (m, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set port");
    goto error;
  }

  if (gst_sdp_media_add_format (m, SDP_MEDIA_DTLS_SCTP_FMT) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set format");
    goto error;
  }

  if (gst_sdp_media_add_attribute (m, "setup", "actpass") != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to set attribute setup:actpass");
    goto error;
  }

  /* Format parameters when protocol is DTLS/SCTP carries the SCTP port */
  /* number and the mandatory "a=sctpmap:" attribute contains the actual */
  /* media format within the protocol parameter. */
  for (i = 0; i < m->fmts->len; i++) {
    gchar *sctp_port, *attr;

    sctp_port = g_array_index (m->fmts, gchar *, i);
    attr = g_strdup_printf ("%s webrtc-datachannel %d",
        sctp_port, DEFAULT_STREAMS_N);

    if (gst_sdp_media_add_attribute (m, "sctpmap", attr) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Unable to set attribute sctpman:%s", attr);
      g_free (attr);
      goto error;
    }

    g_free (attr);
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

static const gchar *
get_sctpmap_value (const GstSDPMedia * media, const gchar * fmt)
{
  const gchar *val = NULL;
  guint i;

  for (i = 0;; i++) {
    gchar **attrs;

    val = gst_sdp_media_get_attribute_val_n (media, "sctpmap", i);

    if (val == NULL) {
      return NULL;
    }

    attrs = g_strsplit (val, " ", 0);

    if (g_strcmp0 (fmt, attrs[0] /* format */ ) == 0) {
      g_strfreev (attrs);
      return val;
    }

    g_strfreev (attrs);
  }
}

static gboolean
format_supported (const GstSDPMedia * media, const gchar * fmt)
{
  const gchar *val;
  gchar **attrs;
  gboolean ret;

  val = get_sctpmap_value (media, fmt);

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
    val = get_sctpmap_value (offer, fmt);

    if (val == NULL) {
      GST_WARNING ("Not sctpmap:%s attribute found in offer", fmt);
      continue;
    }

    if (gst_sdp_media_add_attribute (answer, "sctpmap", val) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not add attribute sctpmap:%s", val);
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
    val = get_sctpmap_value (offer, fmt);

    if (val == NULL) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Not sctpmap:%s attribute found in offer", fmt);
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

      if (gst_sdp_media_add_attribute (answer, attrs[1], val) != GST_SDP_OK) {
        g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
            "Can add attribute %s:%s", attrs[1], val);
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
    val = get_sctpmap_value (media, fmt);

    if (val == NULL) {
      GST_WARNING ("Not sctpmap:%s attribute found in offer", fmt);
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
add_setup_attribute (GstSDPMedia * answer, const GstSDPAttribute * attr,
    GError ** error)
{
  const gchar *setup;

  if (g_strcmp0 (attr->key, "setup") != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "%s is not a setup attribute", attr->key);
    return FALSE;
  }

  if (g_strcmp0 (attr->value, "active") == 0) {
    setup = "passive";
  } else if (g_strcmp0 (attr->value, "passive") == 0) {
    setup = "active";
  } else if (g_strcmp0 (attr->value, "actpass") == 0) {
    setup = "active";
  } else {
    setup = "holdconn";
  }

  if (gst_sdp_media_add_attribute (answer, attr->key, setup) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not add attribute %s:%s", attr->key, setup);
    return FALSE;
  }

  return TRUE;
}

static gboolean
add_missing_attrs (const GstSDPMedia * offer, GstSDPMedia * answer,
    GError ** error)
{
  guint i, len;

  len = gst_sdp_media_attributes_len (offer);

  for (i = 0; i < len; i++) {
    const GstSDPAttribute *attr;

    attr = gst_sdp_media_get_attribute (offer, i);

    if (g_strcmp0 (attr->key, "sctpmap") == 0 || is_subproto (offer, attr->key)) {
      /* ignore */
      continue;
    }

    if (g_strcmp0 (attr->key, "setup") == 0) {
      /* follow rules defined in RFC4145 */
      if (!add_setup_attribute (answer, attr, error)) {
        return FALSE;
      }
      continue;
    }

    if (g_strcmp0 (attr->key, "connection") == 0) {
      /* TODO: Implment a mechanism that allows us to know if a */
      /* new connection is gonna be required or an existing one */
      /* can be used. By default we always create a new one. */
      if (gst_sdp_media_add_attribute (answer, "connection",
              "new") != GST_SDP_OK) {
        g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
            SDP_AGENT_UNEXPECTED_ERROR, "Can not add attribute connection:new");
        return FALSE;
      }
      continue;
    }

    if (gst_sdp_media_add_attribute (answer, attr->key,
            attr->value) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can not add attribute %s:%s", attr->key, attr->value);
      return FALSE;
    }
  }

  return TRUE;
}

GstSDPMedia *
kms_sdp_sctp_media_handler_create_answer (KmsSdpMediaHandler * handler,
    const GstSDPMedia * offer, GError ** error)
{
  GstSDPMedia *m = NULL;
  gchar *proto = NULL;
  guint i, len;

  if (g_strcmp0 (gst_sdp_media_get_media (offer), "application") != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_MEDIA,
        "Unsupported %s media", gst_sdp_media_get_media (offer));
    goto error;
  }

  g_object_get (handler, "proto", &proto, NULL);

  if (g_strcmp0 (proto, gst_sdp_media_get_proto (offer)) != 0) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PROTOCOL,
        "Unexpected media protocol %s", gst_sdp_media_get_proto (offer));
    goto error;
  }

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Unable to create %s media answer", gst_sdp_media_get_media (offer));
    goto error;
  }

  if (gst_sdp_media_set_media (m,
          gst_sdp_media_get_media (offer)) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set %s media ttribute", gst_sdp_media_get_media (offer));
    goto error;
  }

  if (gst_sdp_media_set_proto (m, proto) != GST_SDP_OK) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
        "Can not set proto %s attribute", proto);
    goto error;
  }

  if (gst_sdp_media_set_port_info (m, 1, 1) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set port attribute");
    goto error;
  }

  len = gst_sdp_media_formats_len (offer);

  /* Set only supported media formats in answer */
  for (i = 0; i < len; i++) {
    const gchar *fmt;

    fmt = gst_sdp_media_get_format (offer, i);

    if (!format_supported (offer, fmt)) {
      continue;
    }

    if (gst_sdp_media_add_format (m, fmt) != GST_SDP_OK) {
      g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
          "Can add format %s", fmt);
      goto error;
    }
  }

  if (!add_supported_sctmap_attrs (offer, m, error)) {
    goto error;
  }

  if (!add_supported_subproto_attrs (offer, m, error)) {
    goto error;
  }

  if (!add_missing_attrs (offer, m, error) != GST_SDP_OK) {
    goto error;
  }

  g_free (proto);

  return m;

error:
  if (proto != NULL) {
    g_free (proto);
  }

  if (m != NULL) {
    gst_sdp_media_free (m);
  }

  return NULL;
}

static void
kms_sdp_sctp_media_handler_class_init (KmsSdpSctpMediaHandlerClass * klass)
{
  GObjectClass *gobject_class;
  KmsSdpMediaHandlerClass *handler_class;

  gobject_class = G_OBJECT_CLASS (klass);
  handler_class = KMS_SDP_MEDIA_HANDLER_CLASS (klass);

  gobject_class->constructor = kms_sdp_sctp_media_handler_constructor;
  handler_class->create_offer = kms_sdp_sctp_media_handler_create_offer;
  handler_class->create_answer = kms_sdp_sctp_media_handler_create_answer;
}

static void
kms_sdp_sctp_media_handler_init (KmsSdpSctpMediaHandler * self)
{
  /* Nothing to do here */
}

KmsSdpSctpMediaHandler *
kms_sdp_sctp_media_handler_new (void)
{
  KmsSdpSctpMediaHandler *handler;

  handler =
      KMS_SDP_SCTP_MEDIA_HANDLER (g_object_new (KMS_TYPE_SDP_SCTP_MEDIA_HANDLER,
          NULL));

  return handler;
}
