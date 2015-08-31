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
#include "sdp_utils.h"
#include "kmssdprtpsavpfmediahandler.h"

#define OBJECT_NAME "rtpsavpfmediahandler"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_rtp_savpf_media_handler_debug_category);
#define GST_CAT_DEFAULT kms_sdp_rtp_savpf_media_handler_debug_category

#define parent_class kms_sdp_rtp_savpf_media_handler_parent_class

G_DEFINE_TYPE_WITH_CODE (KmsSdpRtpSavpfMediaHandler,
    kms_sdp_rtp_savpf_media_handler, KMS_TYPE_SDP_RTP_AVPF_MEDIA_HANDLER,
    GST_DEBUG_CATEGORY_INIT (kms_sdp_rtp_savpf_media_handler_debug_category,
        OBJECT_NAME, 0, "debug category for sdp rtp savpf media_handler"));

#define SDP_MEDIA_RTP_SAVPF_PROTO "RTP/SAVPF"
#define SDP_MEDIA_UDP_TLS_PROTO_INFO "UDP/TLS/"

static GObject *
kms_sdp_rtp_savpf_media_handler_constructor (GType gtype, guint n_properties,
    GObjectConstructParam * properties)
{
  GObjectConstructParam *property;
  gchar const *name;
  GObject *object;
  guint i;

  for (i = 0, property = properties; i < n_properties; ++i, ++property) {
    name = g_param_spec_get_name (property->pspec);
    if (g_strcmp0 (name, "proto") == 0) {
      if (g_value_get_string (property->value) == NULL) {
        /* change G_PARAM_CONSTRUCT_ONLY value */
        g_value_set_string (property->value, SDP_MEDIA_RTP_SAVPF_PROTO);
      }
    }
  }

  object =
      G_OBJECT_CLASS (parent_class)->constructor (gtype, n_properties,
      properties);

  return object;
}

static gboolean
kms_sdp_rtp_savpf_media_handler_manage_protocol (KmsSdpMediaHandler * handler,
    const gchar * protocol)
{
  GRegex *regex;
  gboolean ret;

  /* Support both RTP/SAVPF and UDP/TLS/RTP/SAVPF */

  regex =
      g_regex_new ("(" SDP_MEDIA_UDP_TLS_PROTO_INFO ")?"
      SDP_MEDIA_RTP_SAVPF_PROTO, 0, 0, NULL);
  ret = g_regex_match (regex, protocol, G_REGEX_MATCH_ANCHORED, NULL);
  g_regex_unref (regex);

  return ret;
}

static void
kms_sdp_rtp_savpf_media_handler_class_init (KmsSdpRtpSavpfMediaHandlerClass *
    klass)
{
  GObjectClass *gobject_class;
  KmsSdpMediaHandlerClass *handler_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = kms_sdp_rtp_savpf_media_handler_constructor;

  handler_class = KMS_SDP_MEDIA_HANDLER_CLASS (klass);

  handler_class->manage_protocol =
      kms_sdp_rtp_savpf_media_handler_manage_protocol;
}

static void
kms_sdp_rtp_savpf_media_handler_init (KmsSdpRtpSavpfMediaHandler * self)
{
  /* Nothing to do here */
}

KmsSdpRtpSavpfMediaHandler *
kms_sdp_rtp_savpf_media_handler_new ()
{
  KmsSdpRtpSavpfMediaHandler *handler;

  handler =
      KMS_SDP_RTP_SAVPF_MEDIA_HANDLER (g_object_new
      (KMS_TYPE_SDP_RTP_SAVPF_MEDIA_HANDLER, NULL));

  return handler;
}
