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
    const gchar * media)
{
  GstSDPMedia *m = NULL;
  guint i;

  if (gst_sdp_media_new (&m) != GST_SDP_OK) {
    GST_WARNING_OBJECT (handler, "Unable to create %s media", media);
    goto error;
  }

  gst_sdp_media_set_media (m, media);
  gst_sdp_media_set_proto (m, SDP_MEDIA_DTLS_SCTP_PROTO);
  gst_sdp_media_set_port_info (m, 1, 1);

  gst_sdp_media_add_format (m, SDP_MEDIA_DTLS_SCTP_FMT);

  gst_sdp_media_add_attribute (m, "setup", "actpass");

  /* Format parameters when protocol is DTLS/SCTP carries the SCTP port */
  /* number and the mandatory "a=sctpmap:" attribute contains the actual */
  /* media format within the protocol parameter. */
  for (i = 0; i < m->fmts->len; i++) {
    gchar *sctp_port, *attr;

    sctp_port = g_array_index (m->fmts, gchar *, i);
    attr = g_strdup_printf ("%s webrtc-datachannel %d",
        sctp_port, DEFAULT_STREAMS_N);
    gst_sdp_media_add_attribute (m, "sctpmap", attr);
    g_free (attr);
  }

  return m;

error:
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
