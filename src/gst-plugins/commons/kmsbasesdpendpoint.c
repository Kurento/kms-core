/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#include "kmsbasesdpendpoint.h"
#include "kmsagnosticcaps.h"
#include "kms-core-marshal.h"
#include "sdp_utils.h"

#define PLUGIN_NAME "base_sdp_endpoint"

GST_DEBUG_CATEGORY_STATIC (kms_base_sdp_endpoint_debug);
#define GST_CAT_DEFAULT kms_base_sdp_endpoint_debug

#define kms_base_sdp_endpoint_parent_class parent_class
G_DEFINE_TYPE (KmsBaseSdpEndpoint, kms_base_sdp_endpoint, KMS_TYPE_ELEMENT);

#define KMS_BASE_SDP_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                   \
    (obj),                                        \
    KMS_TYPE_BASE_SDP_ENDPOINT,                   \
    KmsBaseSdpEndpointPrivate                     \
  )                                               \
)

#define USE_IPV6_DEFAULT FALSE
#define MAX_VIDEO_RECV_BW_DEFAULT 500

/* Signals and args */
enum
{
  SIGNAL_GENERATE_OFFER,
  SIGNAL_PROCESS_OFFER,
  SIGNAL_PROCESS_ANSWER,
  LAST_SIGNAL
};

static guint kms_base_sdp_endpoint_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_USE_IPV6,
  PROP_PATTERN_SDP,
  /* TODO: remove begin */
  PROP_LOCAL_OFFER_SDP,
  PROP_LOCAL_ANSWER_SDP,
  PROP_REMOTE_OFFER_SDP,
  PROP_REMOTE_ANSWER_SDP,
  /* TODO: remove end */
  PROP_LOCAL_SDP,
  PROP_REMOTE_SDP,
  PROP_MAX_VIDEO_RECV_BW,
  N_PROPERTIES
};

struct _KmsBaseSdpEndpointPrivate
{
  GstSDPMessage *pattern_sdp;

  GstSDPMessage *local_offer_sdp;
  GstSDPMessage *local_answer_sdp;

  GstSDPMessage *remote_offer_sdp;
  GstSDPMessage *remote_answer_sdp;

  GstSDPMessage *local_sdp;
  GstSDPMessage *remote_sdp;

  gboolean use_ipv6;

  guint max_video_recv_bw;
};

static void
kms_base_sdp_endpoint_release_pattern_sdp (KmsBaseSdpEndpoint * self)
{
  if (self->priv->pattern_sdp == NULL) {
    return;
  }

  gst_sdp_message_free (self->priv->pattern_sdp);
  self->priv->pattern_sdp = NULL;
}

static void
kms_base_sdp_endpoint_release_local_offer_sdp (KmsBaseSdpEndpoint * self)
{
  if (self->priv->local_offer_sdp == NULL) {
    return;
  }

  gst_sdp_message_free (self->priv->local_offer_sdp);
  self->priv->local_offer_sdp = NULL;
}

static void
kms_base_sdp_endpoint_release_local_answer_sdp (KmsBaseSdpEndpoint * self)
{
  if (self->priv->local_answer_sdp == NULL) {
    return;
  }

  gst_sdp_message_free (self->priv->local_answer_sdp);
  self->priv->local_answer_sdp = NULL;
}

static void
kms_base_sdp_endpoint_release_remote_offer_sdp (KmsBaseSdpEndpoint * self)
{
  if (self->priv->remote_offer_sdp == NULL) {
    return;
  }

  gst_sdp_message_free (self->priv->remote_offer_sdp);
  self->priv->remote_offer_sdp = NULL;
}

static void
kms_base_sdp_endpoint_release_remote_answer_sdp (KmsBaseSdpEndpoint * self)
{
  if (self->priv->remote_answer_sdp == NULL) {
    return;
  }

  gst_sdp_message_free (self->priv->remote_answer_sdp);
  self->priv->remote_answer_sdp = NULL;
}

static void
kms_base_sdp_endpoint_set_local_offer_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * offer)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_local_offer_sdp (self);
  gst_sdp_message_copy (offer, &self->priv->local_offer_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "local-offer-sdp");
}

static void
kms_base_sdp_endpoint_set_remote_offer_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * offer)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_remote_offer_sdp (self);
  gst_sdp_message_copy (offer, &self->priv->remote_offer_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "remote-offer-sdp");
}

static void
kms_base_sdp_endpoint_set_local_answer_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * offer)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_local_answer_sdp (self);
  gst_sdp_message_copy (offer, &self->priv->local_answer_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "local-answer-sdp");
}

static void
kms_base_sdp_endpoint_set_remote_answer_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * offer)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_remote_answer_sdp (self);
  gst_sdp_message_copy (offer, &self->priv->remote_answer_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "remote-answer-sdp");
}

static void
kms_base_sdp_endpoint_release_sdp (GstSDPMessage ** sdp)
{
  if (*sdp == NULL) {
    return;
  }

  gst_sdp_message_free (*sdp);
  *sdp = NULL;
}

static void
kms_base_sdp_endpoint_set_local_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * local_sdp)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_sdp (&self->priv->local_sdp);
  gst_sdp_message_copy (local_sdp, &self->priv->local_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "local-sdp");
}

static void
kms_base_sdp_endpoint_set_remote_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * remote_sdp)
{
  KMS_ELEMENT_LOCK (self);
  kms_base_sdp_endpoint_release_sdp (&self->priv->remote_sdp);
  gst_sdp_message_copy (remote_sdp, &self->priv->remote_sdp);
  KMS_ELEMENT_UNLOCK (self);
  g_object_notify (G_OBJECT (self), "remote-sdp");
}

static void
kms_base_sdp_endpoint_start_transport_send (KmsBaseSdpEndpoint *
    self, const GstSDPMessage * offer,
    const GstSDPMessage * answer, gboolean local_offer)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  /* Defalut function, do nothing */
  if (base_sdp_endpoint_class->start_transport_send ==
      kms_base_sdp_endpoint_start_transport_send) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement \"start_transport_send\"",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }
}

static void
kms_base_sdp_endpoint_connect_input_elements (KmsBaseSdpEndpoint *
    self, const GstSDPMessage * answer)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  /* Defalut function, do nothing */
  if (base_sdp_endpoint_class->connect_input_elements ==
      kms_base_sdp_endpoint_connect_input_elements) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement \"connect_input_elements\"",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }
}

static void
kms_base_sdp_endpoint_start_media (KmsBaseSdpEndpoint * self,
    const GstSDPMessage * offer, const GstSDPMessage * answer,
    gboolean local_offer)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  GST_DEBUG_OBJECT (self, "Start media");

  base_sdp_endpoint_class->start_transport_send (self, offer,
      answer, local_offer);

  base_sdp_endpoint_class->connect_input_elements (self, answer);
}

static gboolean
kms_base_sdp_endpoint_set_transport_to_sdp (KmsBaseSdpEndpoint *
    self, GstSDPMessage * msg)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (base_sdp_endpoint_class->set_transport_to_sdp ==
      kms_base_sdp_endpoint_set_transport_to_sdp) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement \"set_transport_to_sdp\"",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }

  /* Defalut function, do nothing */
  return TRUE;
}

static GstSDPMessage *
kms_base_sdp_endpoint_generate_offer (KmsBaseSdpEndpoint * self)
{
  GstSDPMessage *offer = NULL;
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  GST_DEBUG_OBJECT (self, "generate_offer");

  KMS_ELEMENT_LOCK (self);

  if (self->priv->pattern_sdp != NULL) {
    gst_sdp_message_copy (self->priv->pattern_sdp, &offer);
  }

  if (offer == NULL) {
    goto end;
  }

  if (!base_sdp_endpoint_class->set_transport_to_sdp (self, offer)) {
    gst_sdp_message_free (offer);
    offer = NULL;
    goto end;
  }

  kms_base_sdp_endpoint_set_local_offer_sdp (self, offer);
  sdp_utils_set_max_video_recv_bw (offer, self->priv->max_video_recv_bw);
  kms_base_sdp_endpoint_set_local_sdp (self, offer);

end:
  KMS_ELEMENT_UNLOCK (self);

  return offer;
}

static GstSDPMessage *
kms_base_sdp_endpoint_process_offer (KmsBaseSdpEndpoint * self,
    GstSDPMessage * offer)
{
  GstSDPMessage *answer = NULL, *intersec_offer, *intersect_answer = NULL;
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  kms_base_sdp_endpoint_set_remote_offer_sdp (self, offer);
  GST_DEBUG_OBJECT (self, "process_offer");

  KMS_ELEMENT_LOCK (self);

  kms_base_sdp_endpoint_set_remote_sdp (self, offer);

  if (self->priv->pattern_sdp != NULL) {
    gst_sdp_message_copy (self->priv->pattern_sdp, &answer);
  }

  if (answer == NULL) {
    goto end;
  }

  if (!base_sdp_endpoint_class->set_transport_to_sdp (self, answer)) {
    gst_sdp_message_free (answer);
    goto end;
  }

  if (sdp_utils_intersect_sdp_messages (offer, answer, &intersec_offer,
          &intersect_answer) != GST_SDP_OK) {
    gst_sdp_message_free (answer);
    intersect_answer = NULL;
    goto end;
  }
  gst_sdp_message_free (intersec_offer);
  gst_sdp_message_free (answer);

  kms_base_sdp_endpoint_set_local_answer_sdp (self, intersect_answer);

  kms_base_sdp_endpoint_start_media (self, offer, intersect_answer, FALSE);

  sdp_utils_set_max_video_recv_bw (intersect_answer,
      self->priv->max_video_recv_bw);
  kms_base_sdp_endpoint_set_local_sdp (self, intersect_answer);

end:
  KMS_ELEMENT_UNLOCK (self);

  return intersect_answer;
}

static void
kms_base_sdp_endpoint_process_answer (KmsBaseSdpEndpoint * self,
    GstSDPMessage * answer)
{
  GST_DEBUG_OBJECT (self, "process_answer");

  KMS_ELEMENT_LOCK (self);

  if (self->priv->local_offer_sdp == NULL) {
    // TODO: This should raise an error
    GST_ERROR_OBJECT (self, "Answer received without a local offer generated");
    goto end;
  }

  kms_base_sdp_endpoint_set_remote_answer_sdp (self, answer);
  kms_base_sdp_endpoint_set_remote_sdp (self, answer);

  kms_base_sdp_endpoint_start_media (self,
      self->priv->local_offer_sdp, answer, TRUE);

end:
  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_sdp_endpoint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsBaseSdpEndpoint *self = KMS_BASE_SDP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
    case PROP_PATTERN_SDP:
      kms_base_sdp_endpoint_release_pattern_sdp (self);
      self->priv->pattern_sdp = g_value_dup_boxed (value);
      break;
    case PROP_USE_IPV6:
      self->priv->use_ipv6 = g_value_get_boolean (value);
      break;
    case PROP_MAX_VIDEO_RECV_BW:
      self->priv->max_video_recv_bw = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_sdp_endpoint_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsBaseSdpEndpoint *self = KMS_BASE_SDP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
    case PROP_USE_IPV6:
      g_value_set_boolean (value, self->priv->use_ipv6);
      break;
    case PROP_PATTERN_SDP:
      g_value_set_boxed (value, self->priv->pattern_sdp);
      break;
    case PROP_LOCAL_OFFER_SDP:
      g_value_set_boxed (value, self->priv->local_offer_sdp);
      break;
    case PROP_LOCAL_ANSWER_SDP:
      g_value_set_boxed (value, self->priv->local_answer_sdp);
      break;
    case PROP_REMOTE_OFFER_SDP:
      g_value_set_boxed (value, self->priv->remote_offer_sdp);
      break;
    case PROP_REMOTE_ANSWER_SDP:
      g_value_set_boxed (value, self->priv->remote_answer_sdp);
      break;
    case PROP_MAX_VIDEO_RECV_BW:
      g_value_set_uint (value, self->priv->max_video_recv_bw);
      break;
    case PROP_LOCAL_SDP:
      g_value_set_boxed (value, self->priv->local_sdp);
      break;
    case PROP_REMOTE_SDP:
      g_value_set_boxed (value, self->priv->remote_sdp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_base_sdp_endpoint_finalize (GObject * object)
{
  KmsBaseSdpEndpoint *self = KMS_BASE_SDP_ENDPOINT (object);

  kms_base_sdp_endpoint_release_pattern_sdp (self);
  kms_base_sdp_endpoint_release_local_offer_sdp (self);
  kms_base_sdp_endpoint_release_local_answer_sdp (self);
  kms_base_sdp_endpoint_release_remote_offer_sdp (self);
  kms_base_sdp_endpoint_release_remote_answer_sdp (self);

  kms_base_sdp_endpoint_release_sdp (&self->priv->local_sdp);
  kms_base_sdp_endpoint_release_sdp (&self->priv->remote_sdp);

  /* chain up */
  G_OBJECT_CLASS (kms_base_sdp_endpoint_parent_class)->finalize (object);
}

static void
kms_base_sdp_endpoint_class_init (KmsBaseSdpEndpointClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = kms_base_sdp_endpoint_get_property;
  gobject_class->set_property = kms_base_sdp_endpoint_set_property;
  gobject_class->finalize = kms_base_sdp_endpoint_finalize;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "BaseSdpEndpoint",
      "Base/Bin/BaseSdpEndpoint",
      "Base class for sdpEndpoints",
      "José Antonio Santos Cadenas <santoscadenas@kurento.com>");

  klass->set_transport_to_sdp = kms_base_sdp_endpoint_set_transport_to_sdp;
  klass->start_transport_send = kms_base_sdp_endpoint_start_transport_send;
  klass->connect_input_elements = kms_base_sdp_endpoint_connect_input_elements;

  klass->generate_offer = kms_base_sdp_endpoint_generate_offer;
  klass->process_offer = kms_base_sdp_endpoint_process_offer;
  klass->process_answer = kms_base_sdp_endpoint_process_answer;

  /* Signals initialization */
  kms_base_sdp_endpoint_signals[SIGNAL_GENERATE_OFFER] =
      g_signal_new ("generate-offer",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseSdpEndpointClass, generate_offer), NULL, NULL,
      __kms_core_marshal_BOXED__VOID, GST_TYPE_SDP_MESSAGE, 0);

  kms_base_sdp_endpoint_signals[SIGNAL_PROCESS_OFFER] =
      g_signal_new ("process-offer",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseSdpEndpointClass, process_offer), NULL, NULL,
      __kms_core_marshal_BOXED__BOXED, GST_TYPE_SDP_MESSAGE, 1,
      GST_TYPE_SDP_MESSAGE);

  kms_base_sdp_endpoint_signals[SIGNAL_PROCESS_ANSWER] =
      g_signal_new ("process-answer",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsBaseSdpEndpointClass, process_answer), NULL, NULL,
      __kms_core_marshal_VOID__BOXED, G_TYPE_NONE, 1, GST_TYPE_SDP_MESSAGE);

  /* Properties initialization */
  g_object_class_install_property (gobject_class, PROP_USE_IPV6,
      g_param_spec_boolean ("use-ipv6", "Use ipv6 in SDPs",
          "Use ipv6 addresses in generated sdp offers and answers",
          USE_IPV6_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PATTERN_SDP,
      g_param_spec_boxed ("pattern-sdp", "Pattern to create local sdps",
          "Pattern to create \"local-offer-sdp\" and \"local-answer-sdp\"",
          GST_TYPE_SDP_MESSAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LOCAL_OFFER_SDP,
      g_param_spec_boxed ("local-offer-sdp", "Local offer sdp",
          "The local generated offer, negotiated with \"remote-answer-sdp\"",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_OFFER_SDP,
      g_param_spec_boxed ("remote-offer-sdp", "Remote offer sdp",
          "The remote offer, negotiated with \"local-answer-sdp\"",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOCAL_ANSWER_SDP,
      g_param_spec_boxed ("local-answer-sdp", "Local answer sdp",
          "The local negotiated answer, negotiated with \"remote-offer-sdp\"",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_ANSWER_SDP,
      g_param_spec_boxed ("remote-answer-sdp", "Remote answer sdp",
          "The remote answer, negotiated with \"local-offer-sdp\"",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOCAL_SDP,
      g_param_spec_boxed ("local-sdp", "Local SDP",
          "The local SDP",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_SDP,
      g_param_spec_boxed ("remote-sdp", "Remote SDP",
          "The remote SDP",
          GST_TYPE_SDP_MESSAGE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_VIDEO_RECV_BW,
      g_param_spec_uint ("max-video-recv-bandwidth",
          "Maximum video bandwidth for receiving",
          "Maximum video bandwidth for receiving. Unit: kbps(kilobits per second). 0: unlimited",
          0, G_MAXUINT32, MAX_VIDEO_RECV_BW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (KmsBaseSdpEndpointPrivate));
}

static void
kms_base_sdp_endpoint_init (KmsBaseSdpEndpoint * self)
{
  self->priv = KMS_BASE_SDP_ENDPOINT_GET_PRIVATE (self);

  self->priv->use_ipv6 = USE_IPV6_DEFAULT;
  self->priv->max_video_recv_bw = MAX_VIDEO_RECV_BW_DEFAULT;
}
