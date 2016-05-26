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
#include "kmssdppayloadmanager.h"
#include "kmsisdppayloadmanager.h"

#define OBJECT_NAME "sdppayloadmanager"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_payload_manager_debug_category);
#define GST_CAT_DEFAULT kms_sdp_payload_manager_debug_category

#define parent_class kms_sdp_payload_manager_parent_class

static void
kms_i_sdp_playload_manager_iface_init (KmsISdpPayloadManagerInterface * iface);

G_DEFINE_TYPE_WITH_CODE (KmsSdpPayloadManager,
    kms_sdp_payload_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_SDP_PAYLOAD_MANAGER,
        kms_i_sdp_playload_manager_iface_init);
    GST_DEBUG_CATEGORY_INIT (kms_sdp_payload_manager_debug_category,
        OBJECT_NAME, 0, "debug category for sdp rtp savpf media_handler"));

#define MIN_DYNAMIC_PAYLOAD 96
#define MAX_DYNAMIC_PAYLOAD 127

#define KMS_SDP_PAYLOAD_MANAGER_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                     \
    (obj),                                          \
    KMS_TYPE_SDP_PAYLOAD_MANAGER,                   \
    KmsSdpPayloadManagerPrivate                     \
  )                                                 \
)

struct _KmsSdpPayloadManagerPrivate
{
  guint counter;                /* atomic */
};

static void
kms_sdp_payload_manager_class_init (KmsSdpPayloadManagerClass * klass)
{
  g_type_class_add_private (klass, sizeof (KmsSdpPayloadManagerPrivate));
}

static void
kms_sdp_payload_manager_init (KmsSdpPayloadManager * self)
{
  self->priv = KMS_SDP_PAYLOAD_MANAGER_GET_PRIVATE (self);
  self->priv->counter = MIN_DYNAMIC_PAYLOAD;
}

KmsSdpPayloadManager *
kms_sdp_payload_manager_new ()
{
  KmsSdpPayloadManager *obj;

  obj =
      KMS_SDP_PAYLOAD_MANAGER (g_object_new (KMS_TYPE_SDP_PAYLOAD_MANAGER,
          NULL));

  return obj;
}

gint
kms_sdp_payload_manager_get_dynamic_pt (KmsISdpPayloadManager * obj,
    GError ** error)
{
  KmsSdpPayloadManager *self = KMS_SDP_PAYLOAD_MANAGER (obj);
  gint val;

  val = g_atomic_int_add (&self->priv->counter, 1);

  if (val <= MAX_DYNAMIC_PAYLOAD) {
    return val;
  }

  g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
      "Not more dynamic payload types available");

  return -1;
}

static void
kms_i_sdp_playload_manager_iface_init (KmsISdpPayloadManagerInterface * iface)
{
  iface->get_dynamic_pt = kms_sdp_payload_manager_get_dynamic_pt;
}
