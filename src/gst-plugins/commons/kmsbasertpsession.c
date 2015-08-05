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
#  include <config.h>
#endif

#include "kmsbasertpsession.h"
#include "kmsutils.h"

#define GST_DEFAULT_NAME "kmsbasertpsession"
#define GST_CAT_DEFAULT kms_base_rtp_session_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_base_rtp_session_parent_class parent_class
G_DEFINE_TYPE (KmsBaseRtpSession, kms_base_rtp_session, KMS_TYPE_SDP_SESSION);

KmsBaseRtpSession *
kms_base_rtp_session_new (KmsBaseSdpEndpoint * ep, guint id)
{
  GObject *obj;
  KmsBaseRtpSession *self;
  KmsSdpSession *sdp_sess;

  obj = g_object_new (KMS_TYPE_BASE_RTP_SESSION, NULL);
  self = KMS_BASE_RTP_SESSION (obj);
  sdp_sess = KMS_SDP_SESSION (self);
  KMS_SDP_SESSION_CLASS
      (kms_base_rtp_session_parent_class)->post_constructor (sdp_sess, ep, id);

  return self;
}

KmsIRtpConnection *
kms_base_rtp_session_get_connection_by_name (KmsBaseRtpSession * self,
    const gchar * name)
{
  gpointer *conn;

  conn = g_hash_table_lookup (self->conns, name);
  if (conn == NULL) {
    return NULL;
  }

  return KMS_I_RTP_CONNECTION (conn);
}

KmsIRtpConnection *
kms_base_rtp_session_get_connection (KmsBaseRtpSession * self,
    SdpMediaConfig * mconf)
{
  gchar *name = kms_utils_create_connection_name_from_media_config (mconf);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_session_get_connection_by_name (self, name);
  if (conn == NULL) {
    GST_WARNING_OBJECT (self, "Connection '%s' not found", name);
    g_free (name);
    return NULL;
  }
  g_free (name);

  return conn;
}

static void
kms_base_rtp_session_finalize (GObject * object)
{
  KmsBaseRtpSession *self = KMS_BASE_RTP_SESSION (object);

  g_hash_table_destroy (self->conns);

  /* chain up */
  G_OBJECT_CLASS (kms_base_rtp_session_parent_class)->finalize (object);
}

static void
kms_base_rtp_session_init (KmsBaseRtpSession * self)
{
  self->conns =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
kms_base_rtp_session_class_init (KmsBaseRtpSessionClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gobject_class->finalize = kms_base_rtp_session_finalize;

  gst_element_class_set_details_simple (gstelement_class,
      "BaseRtpSession",
      "Generic",
      "Base bin to manage elements related with a RTP session.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");
}
