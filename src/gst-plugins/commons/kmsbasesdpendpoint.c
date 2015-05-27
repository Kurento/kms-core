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
#include "sdpagent/kmssdprtpavpmediahandler.h"
#include "sdpagent/kmssdppayloadmanager.h"

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

#define GST_VALUE_HOLDS_STRUCTURE(x)            (G_VALUE_HOLDS((x), _gst_structure_type))

/* Signals and args */
enum
{
  SIGNAL_GENERATE_OFFER,
  SIGNAL_PROCESS_OFFER,
  SIGNAL_PROCESS_ANSWER,
  LAST_SIGNAL
};

static guint kms_base_sdp_endpoint_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_BUNDLE    FALSE
#define DEFAULT_NUM_AUDIO_MEDIAS    0
#define DEFAULT_NUM_VIDEO_MEDIAS    0

enum
{
  PROP_0,
  PROP_BUNDLE,
  PROP_USE_IPV6,
  PROP_LOCAL_SDP,
  PROP_REMOTE_SDP,
  PROP_NUM_AUDIO_MEDIAS,
  PROP_NUM_VIDEO_MEDIAS,
  PROP_AUDIO_CODECS,
  PROP_VIDEO_CODECS,
  PROP_MAX_VIDEO_RECV_BW,
  N_PROPERTIES
};

struct _KmsBaseSdpEndpointPrivate
{
  KmsSdpAgent *agent;
  KmsSdpPayloadManager *ptmanager;

  gboolean bundle;

  SdpMessageContext *local_ctx;
  SdpMessageContext *remote_ctx;
  SdpMessageContext *negotiated_ctx;

  guint max_video_recv_bw;

  guint num_audio_medias;
  guint num_video_medias;
  GArray *audio_codecs;
  GArray *video_codecs;
};

/* Media handler management begin */

static void
kms_base_sdp_endpoint_create_media_handler_impl (KmsBaseSdpEndpoint * self,
    const gchar * media, KmsSdpMediaHandler ** handler)
{
  KmsBaseSdpEndpointClass *klass =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->create_media_handler ==
      kms_base_sdp_endpoint_create_media_handler_impl) {
    GST_WARNING_OBJECT (self, "%s does not reimplement 'create_media_handler'",
        G_OBJECT_CLASS_NAME (klass));
  }
}

static void
kms_base_sdp_endpoint_create_media_handler (KmsBaseSdpEndpoint * self,
    const gchar * media, KmsSdpMediaHandler ** handler)
{
  KmsBaseSdpEndpointClass *klass =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));
  klass->create_media_handler (self, media, handler);

  if (*handler == NULL) {
    /* No media supported */
    return;
  }

  if (KMS_IS_SDP_RTP_AVP_MEDIA_HANDLER (*handler)) {
    KmsSdpRtpAvpMediaHandler *h = KMS_SDP_RTP_AVP_MEDIA_HANDLER (*handler);
    GError *err = NULL;

    kms_sdp_rtp_avp_media_handler_use_payload_manager (h,
        KMS_I_SDP_PAYLOAD_MANAGER (g_object_ref (self->priv->ptmanager)), &err);

    if (self->priv->audio_codecs != NULL) {
      int i;

      for (i = 0; i < self->priv->audio_codecs->len; i++) {
        GValue *v = &g_array_index (self->priv->audio_codecs, GValue, i);
        const GstStructure *s;

        if (!GST_VALUE_HOLDS_STRUCTURE (v)) {
          GST_WARNING_OBJECT (self, "Value into array is not a GstStructure");
          continue;
        }

        s = gst_value_get_structure (v);
        kms_sdp_rtp_avp_media_handler_add_audio_codec (h,
            gst_structure_get_name (s), &err);
      }
    }

    if (self->priv->video_codecs != NULL) {
      int i;

      for (i = 0; i < self->priv->video_codecs->len; i++) {
        GValue *v = &g_array_index (self->priv->video_codecs, GValue, i);
        const GstStructure *s;

        if (!GST_VALUE_HOLDS_STRUCTURE (v)) {
          GST_WARNING_OBJECT (self, "Value into array is not a GstStructure");
          continue;
        }

        s = gst_value_get_structure (v);
        kms_sdp_rtp_avp_media_handler_add_video_codec (h,
            gst_structure_get_name (s), &err);
      }
    }
  }
}

static gboolean
kms_base_sdp_endpoint_add_handler (KmsBaseSdpEndpoint * self,
    const gchar * media, gint bundle_group_id, guint max_recv_bw)
{
  KmsSdpMediaHandler *handler = NULL;
  gint hid;

  kms_base_sdp_endpoint_create_media_handler (self, media, &handler);
  if (handler == NULL) {
    GST_ERROR_OBJECT (self, "Cannot create media handler.");
    return FALSE;
  }

  hid = kms_sdp_agent_add_proto_handler (self->priv->agent, media, handler);
  if (hid < 0) {
    GST_ERROR_OBJECT (self, "Cannot add media handler.");
    g_object_unref (handler);
    return FALSE;
  }

  if (bundle_group_id != -1) {
    if (!kms_sdp_agent_add_handler_to_group (self->priv->agent, bundle_group_id,
            hid)) {
      GST_ERROR_OBJECT (self, "Cannot add handler to bundle group.");
      return FALSE;
    }
  }

  if (max_recv_bw > 0) {
    kms_sdp_media_handler_add_bandwidth (handler, "AS", max_recv_bw);
  }

  return TRUE;
}

static gboolean
kms_base_sdp_endpoint_init_sdp_handlers (KmsBaseSdpEndpoint * self)
{
  gint gid;
  int i;

  gid = -1;
  if (self->priv->bundle) {
    gid = kms_sdp_agent_crate_bundle_group (self->priv->agent);
    if (gid < 0) {
      GST_ERROR_OBJECT (self, "Cannot create bundle group.");
      return FALSE;
    }
  }

  for (i = 0; i < self->priv->num_audio_medias; i++) {
    if (!kms_base_sdp_endpoint_add_handler (self, "audio", gid, 0)) {
      return FALSE;
    }
  }

  for (i = 0; i < self->priv->num_video_medias; i++) {
    if (!kms_base_sdp_endpoint_add_handler (self, "video", gid,
            self->priv->max_video_recv_bw)) {
      return FALSE;
    }
  }

  return TRUE;
}

/* Media handler management end */

KmsSdpAgent *
kms_base_sdp_endpoint_get_sdp_agent (KmsBaseSdpEndpoint * self)
{
  return self->priv->agent;
}

SdpMessageContext *
kms_base_sdp_endpoint_get_local_sdp_ctx (KmsBaseSdpEndpoint * self)
{
  return self->priv->local_ctx;
}

SdpMessageContext *
kms_base_sdp_endpoint_get_remote_sdp_ctx (KmsBaseSdpEndpoint * self)
{
  return self->priv->remote_ctx;
}

SdpMessageContext *
kms_base_sdp_endpoint_get_negotiated_sdp_ctx (KmsBaseSdpEndpoint * self)
{
  return self->priv->negotiated_ctx;
}

static void
kms_base_sdp_endpoint_start_transport_send (KmsBaseSdpEndpoint * self,
    gboolean offerer)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  /* Default function, do nothing */
  if (base_sdp_endpoint_class->start_transport_send ==
      kms_base_sdp_endpoint_start_transport_send) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement \"start_transport_send\"",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }
}

static void
kms_base_sdp_endpoint_connect_input_elements (KmsBaseSdpEndpoint * self,
    SdpMessageContext * negotiated_ctx)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  /* Default function, do nothing */
  if (base_sdp_endpoint_class->connect_input_elements ==
      kms_base_sdp_endpoint_connect_input_elements) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement \"connect_input_elements\"",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }
}

static void
kms_base_sdp_endpoint_start_media (KmsBaseSdpEndpoint * self, gboolean offerer)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  GST_DEBUG_OBJECT (self, "Start media");

  base_sdp_endpoint_class->start_transport_send (self, offerer);
  base_sdp_endpoint_class->connect_input_elements (self,
      self->priv->negotiated_ctx);
}

static gboolean
kms_base_sdp_endpoint_configure_media_impl (KmsBaseSdpEndpoint *
    self, SdpMediaConfig * mconf)
{
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  if (base_sdp_endpoint_class->configure_media ==
      kms_base_sdp_endpoint_configure_media_impl) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement 'configure_media'",
        G_OBJECT_CLASS_NAME (base_sdp_endpoint_class));
  }

  /* Default function, do nothing */
  return TRUE;
}

static GstSDPMessage *
kms_base_sdp_endpoint_generate_offer (KmsBaseSdpEndpoint * self)
{
  SdpMessageContext *ctx = NULL;
  GstSDPMessage *offer = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "generate_offer");

  KMS_ELEMENT_LOCK (self);

  if (!kms_base_sdp_endpoint_init_sdp_handlers (self)) {
    goto end;
  }

  ctx = kms_sdp_agent_create_offer (self->priv->agent, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error generating offer", err->message);
    g_error_free (err);
    goto end;
  }

  offer = kms_sdp_message_context_pack (ctx, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error generating offer", err->message);
    g_error_free (err);
    goto end;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_OFFER);
  self->priv->local_ctx = ctx;

end:
  KMS_ELEMENT_UNLOCK (self);

  return offer;
}

static GstSDPMessage *
kms_base_sdp_endpoint_process_offer (KmsBaseSdpEndpoint * self,
    GstSDPMessage * offer)
{
  SdpMessageContext *ctx = NULL;
  GstSDPMessage *answer = NULL;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "process_offer");

  KMS_ELEMENT_LOCK (self);

  if (!kms_base_sdp_endpoint_init_sdp_handlers (self)) {
    goto end;
  }

  ctx = kms_sdp_message_context_new_from_sdp (offer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error processing offer", err->message);
    g_error_free (err);
    goto end;
  }
  kms_sdp_message_context_set_type (ctx, KMS_SDP_OFFER);
  self->priv->remote_ctx = ctx;

  ctx = kms_sdp_agent_create_answer (self->priv->agent, offer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error processing offer", err->message);
    g_error_free (err);
    goto end;
  }

  answer = kms_sdp_message_context_pack (ctx, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error processing offer", err->message);
    g_error_free (err);
    kms_sdp_message_context_destroy (ctx);
    goto end;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_ANSWER);
  self->priv->local_ctx = ctx;
  self->priv->negotiated_ctx = ctx;
  kms_base_sdp_endpoint_start_media (self, FALSE);

end:
  KMS_ELEMENT_UNLOCK (self);

  return answer;
}

static void
kms_base_sdp_endpoint_process_answer (KmsBaseSdpEndpoint * self,
    GstSDPMessage * answer)
{
  SdpMessageContext *ctx;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "process_answer");

  KMS_ELEMENT_LOCK (self);

  if (self->priv->local_ctx == NULL) {
    // TODO: This should raise an error
    GST_ERROR_OBJECT (self, "Answer received without a local offer generated");
    goto end;
  }

  ctx = kms_sdp_message_context_new_from_sdp (answer, &err);
  if (err != NULL) {
    GST_ERROR_OBJECT (self, "%s (%s)", "Error processing answer", err->message);
    g_error_free (err);
    goto end;
  }

  kms_sdp_message_context_set_type (ctx, KMS_SDP_ANSWER);
  self->priv->remote_ctx = ctx;
  self->priv->negotiated_ctx = ctx;
  kms_base_sdp_endpoint_start_media (self, TRUE);

end:
  KMS_ELEMENT_UNLOCK (self);
}

static gboolean
kms_base_sdp_endpoint_configure_media (KmsSdpAgent * agent,
    SdpMediaConfig * mconf, gpointer user_data)
{
  KmsBaseSdpEndpoint *self = KMS_BASE_SDP_ENDPOINT (user_data);
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class =
      KMS_BASE_SDP_ENDPOINT_CLASS (G_OBJECT_GET_CLASS (self));

  return base_sdp_endpoint_class->configure_media (self, mconf);
}

static void
kms_base_sdp_endpoint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsBaseSdpEndpoint *self = KMS_BASE_SDP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
    case PROP_BUNDLE:
      self->priv->bundle = g_value_get_boolean (value);
      break;
    case PROP_USE_IPV6:{
      guint v = g_value_get_boolean (value);

      g_object_set (G_OBJECT (self->priv->agent), "use-ipv6", v, NULL);
      break;
    }
    case PROP_MAX_VIDEO_RECV_BW:
      self->priv->max_video_recv_bw = g_value_get_uint (value);
      break;
    case PROP_NUM_AUDIO_MEDIAS:{
      guint n = g_value_get_uint (value);

      if (n > 1) {
        GST_WARNING_OBJECT (self, "Only 1 audio media is supported");
        n = 1;
      }

      self->priv->num_audio_medias = n;
      break;
    }
    case PROP_NUM_VIDEO_MEDIAS:{
      guint n = g_value_get_uint (value);

      if (n > 1) {
        GST_WARNING_OBJECT (self, "Only 1 video media is supported");
        n = 1;
      }

      self->priv->num_video_medias = n;
      break;
    }
    case PROP_AUDIO_CODECS:{
      if (self->priv->audio_codecs != NULL) {
        g_array_free (self->priv->audio_codecs, TRUE);
      }

      self->priv->audio_codecs = g_value_get_boxed (value);
      g_array_set_clear_func (self->priv->audio_codecs,
          (GDestroyNotify) g_value_unset);
      break;
    }
    case PROP_VIDEO_CODECS:{
      if (self->priv->video_codecs != NULL) {
        g_array_free (self->priv->video_codecs, TRUE);
      }

      self->priv->video_codecs = g_value_get_boxed (value);
      g_array_set_clear_func (self->priv->video_codecs,
          (GDestroyNotify) g_value_unset);
      break;
    }
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
    case PROP_BUNDLE:
      g_value_set_boolean (value, self->priv->bundle);
      break;
    case PROP_USE_IPV6:{
      guint ret;

      g_object_get (G_OBJECT (self->priv->agent), "use-ipv6", &ret, NULL);
      g_value_set_boolean (value, ret);
      break;
    }
    case PROP_MAX_VIDEO_RECV_BW:
      g_value_set_uint (value, self->priv->max_video_recv_bw);
      break;
    case PROP_NUM_AUDIO_MEDIAS:
      g_value_set_uint (value, self->priv->num_audio_medias);
      break;
    case PROP_NUM_VIDEO_MEDIAS:
      g_value_set_uint (value, self->priv->num_video_medias);
      break;
    case PROP_LOCAL_SDP:{
      GstSDPMessage *sdp = NULL;
      GError *err = NULL;

      if (self->priv->local_ctx != NULL) {
        sdp = kms_sdp_message_context_pack (self->priv->local_ctx, &err);
        if (err != NULL) {
          GST_ERROR_OBJECT (self, "Error packing local SDP (%s)", err->message);
          g_error_free (err);
        }
      }

      g_value_set_boxed (value, sdp);
      if (sdp != NULL) {
        gst_sdp_message_free (sdp);
      }
      break;
    }
    case PROP_REMOTE_SDP:{
      GstSDPMessage *sdp = NULL;
      GError *err = NULL;

      if (self->priv->remote_ctx != NULL) {
        sdp = kms_sdp_message_context_pack (self->priv->remote_ctx, &err);
        if (err != NULL) {
          GST_ERROR_OBJECT (self, "Error packing remote SDP (%s)",
              err->message);
          g_error_free (err);
        }
      }

      g_value_set_boxed (value, sdp);
      if (sdp != NULL) {
        gst_sdp_message_free (sdp);
      }
      break;
    }
    case PROP_AUDIO_CODECS:
      g_value_set_boxed (value, self->priv->audio_codecs);
      break;
    case PROP_VIDEO_CODECS:
      g_value_set_boxed (value, self->priv->video_codecs);
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

  if (self->priv->local_ctx != NULL) {
    kms_sdp_message_context_destroy (self->priv->local_ctx);
  }

  if (self->priv->remote_ctx != NULL) {
    kms_sdp_message_context_destroy (self->priv->remote_ctx);
  }

  g_clear_object (&self->priv->ptmanager);
  g_clear_object (&self->priv->agent);

  if (self->priv->audio_codecs != NULL) {
    g_array_free (self->priv->audio_codecs, TRUE);
  }

  if (self->priv->video_codecs != NULL) {
    g_array_free (self->priv->video_codecs, TRUE);
  }

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

  /* Media handler management */
  klass->create_media_handler = kms_base_sdp_endpoint_create_media_handler_impl;

  klass->start_transport_send = kms_base_sdp_endpoint_start_transport_send;
  klass->connect_input_elements = kms_base_sdp_endpoint_connect_input_elements;

  klass->configure_media = kms_base_sdp_endpoint_configure_media_impl;

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
  g_object_class_install_property (gobject_class, PROP_BUNDLE,
      g_param_spec_boolean ("bundle", "Bundle media",
          "Bundle media", DEFAULT_BUNDLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_AUDIO_MEDIAS,
      g_param_spec_uint ("num-audio-medias", "Number of audio medias",
          "Number of audio medias to be negotiated",
          0, G_MAXUINT32, DEFAULT_NUM_AUDIO_MEDIAS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_VIDEO_MEDIAS,
      g_param_spec_uint ("num-video-medias", "Number of video medias",
          "Number of video medias to be negotiated",
          0, G_MAXUINT32, DEFAULT_NUM_VIDEO_MEDIAS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_CODECS,
      g_param_spec_boxed ("audio-codecs", "Audio codecs", "Audio codecs",
          G_TYPE_ARRAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_CODECS,
      g_param_spec_boxed ("video-codecs", "Video codecs", "Video codecs",
          G_TYPE_ARRAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_IPV6,
      g_param_spec_boolean ("use-ipv6", "Use ipv6 in SDPs",
          "Use ipv6 addresses in generated sdp offers and answers",
          USE_IPV6_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

  self->priv->bundle = DEFAULT_BUNDLE;

  self->priv->agent = kms_sdp_agent_new ();
  self->priv->ptmanager = kms_sdp_payload_manager_new ();
  kms_sdp_agent_set_configure_media_callback (self->priv->agent,
      kms_base_sdp_endpoint_configure_media, self, NULL);

  self->priv->max_video_recv_bw = MAX_VIDEO_RECV_BW_DEFAULT;
}
