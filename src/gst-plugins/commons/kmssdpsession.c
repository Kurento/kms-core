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

#include "kmssdpsession.h"
#include "kmsutils.h"

#define GST_DEFAULT_NAME "kmssdpsession"
#define GST_CAT_DEFAULT kms_sdp_session_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_sdp_session_parent_class parent_class
G_DEFINE_TYPE (KmsSdpSession, kms_sdp_session, GST_TYPE_BIN);

KmsSdpSession *
kms_sdp_session_new (KmsBaseSdpEndpoint * ep, guint id)
{
  GObject *obj;
  KmsSdpSession *self;

  obj = g_object_new (KMS_TYPE_SDP_SESSION, NULL);
  self = KMS_SDP_SESSION (obj);
  KMS_SDP_SESSION_CLASS (G_OBJECT_GET_CLASS (self))->post_constructor (self, ep,
      id);

  return self;
}

GstSDPMessage *
kms_sdp_session_generate_offer (KmsSdpSession * self)
{
  SdpMessageContext *ctx;
  GstSDPMessage *offer = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Generate offer");

  ctx = kms_sdp_agent_create_offer (self->agent, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error generating offer (%s)", err->message);
    g_error_free (err);
    goto end;
  }

  offer = kms_sdp_message_context_pack (ctx, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error generating offer (%s)", err->message);
    g_error_free (err);
    goto end;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_OFFER);
  self->local_sdp_ctx = ctx;

end:
  return offer;
}

GstSDPMessage *
kms_sdp_session_process_offer (KmsSdpSession * self, GstSDPMessage * offer)
{
  SdpMessageContext *ctx;
  GstSDPMessage *answer = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Process offer");

  ctx = kms_sdp_message_context_new_from_sdp (offer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error processing offer (%s)", err->message);
    g_error_free (err);
    goto end;
  }
  kms_sdp_message_context_set_type (ctx, KMS_SDP_OFFER);
  self->remote_sdp_ctx = ctx;

  ctx = kms_sdp_agent_create_answer (self->agent, offer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error processing offer (%s)", err->message);
    g_error_free (err);
    goto end;
  }

  answer = kms_sdp_message_context_pack (ctx, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error processing offer (%s)", err->message);
    g_error_free (err);
    kms_sdp_message_context_destroy (ctx);
    goto end;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_ANSWER);
  self->local_sdp_ctx = ctx;
  self->neg_sdp_ctx = ctx;

end:
  return answer;
}

void
kms_sdp_session_process_answer (KmsSdpSession * self, GstSDPMessage * answer)
{
  SdpMessageContext *ctx;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Process answer");

  if (self->local_sdp_ctx == NULL) {
    // TODO: This should raise an error
    GST_ERROR_OBJECT (self, "Answer received without a local offer generated");
    return;
  }

  ctx = kms_sdp_message_context_new_from_sdp (answer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "Error processing answer (%s)", err->message);
    g_error_free (err);
    return;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_ANSWER);
  self->remote_sdp_ctx = ctx;
  self->neg_sdp_ctx = ctx;
}

GstSDPMessage *
kms_sdp_session_get_local_sdp (KmsSdpSession * self)
{
  GstSDPMessage *sdp = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Get local SDP");

  if (self->local_sdp_ctx != NULL) {
    sdp = kms_sdp_message_context_pack (self->local_sdp_ctx, &err);
    if (err != NULL) {
      GST_ERROR_OBJECT (self, "Error packing local SDP (%s)", err->message);
      g_error_free (err);
    }
  }

  return sdp;
}

GstSDPMessage *
kms_sdp_session_get_remote_sdp (KmsSdpSession * self)
{
  GstSDPMessage *sdp = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Get remote SDP");

  if (self->remote_sdp_ctx != NULL) {
    sdp = kms_sdp_message_context_pack (self->remote_sdp_ctx, &err);
    if (err != NULL) {
      GST_ERROR_OBJECT (self, "Error packing remote SDP (%s)", err->message);
      g_error_free (err);
    }
  }

  return sdp;
}

void
kms_sdp_session_set_use_ipv6 (KmsSdpSession * self, gboolean use_ipv6)
{
  g_object_set (self->agent, "use-ipv6", use_ipv6, NULL);
}

void
kms_sdp_session_set_addr (KmsSdpSession * self, const gchar * addr)
{
  g_object_set (self->agent, "addr", addr, NULL);
}

static void
kms_sdp_session_finalize (GObject * object)
{
  KmsSdpSession *self = KMS_SDP_SESSION (object);

  if (self->local_sdp_ctx != NULL) {
    kms_sdp_message_context_destroy (self->local_sdp_ctx);
  }

  if (self->remote_sdp_ctx != NULL) {
    kms_sdp_message_context_destroy (self->remote_sdp_ctx);
  }

  g_clear_object (&self->ptmanager);
  g_clear_object (&self->agent);
  g_free (self->id_str);

  /* chain up */
  G_OBJECT_CLASS (kms_sdp_session_parent_class)->finalize (object);
}

static void
kms_sdp_session_post_constructor (KmsSdpSession * self, KmsBaseSdpEndpoint * ep,
    guint id)
{
  self->id = id;
  self->id_str = g_strdup_printf ("%s-sess%d", GST_ELEMENT_NAME (ep), id);
  self->ep = ep;
}

static void
kms_sdp_session_init (KmsSdpSession * self)
{
  self->agent = kms_sdp_agent_new ();
  self->ptmanager = kms_sdp_payload_manager_new ();
}

static void
kms_sdp_session_class_init (KmsSdpSessionClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gobject_class->finalize = kms_sdp_session_finalize;

  klass->post_constructor = kms_sdp_session_post_constructor;

  gst_element_class_set_details_simple (gstelement_class,
      "SdpSession",
      "Generic",
      "Base bin to manage elements related with a SDP session.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");
}
