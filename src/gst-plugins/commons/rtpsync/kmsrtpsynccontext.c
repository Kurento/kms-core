/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
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

#include "kmsrtpsynccontext.h"

#define GST_DEFAULT_NAME "rtpsynccontext"
GST_DEBUG_CATEGORY_STATIC (kms_rtp_sync_context_debug_category);
#define GST_CAT_DEFAULT kms_rtp_sync_context_debug_category

#define parent_class kms_rtp_sync_context_parent_class
G_DEFINE_TYPE (KmsRtpSyncContext, kms_rtp_sync_context, G_TYPE_OBJECT);

#define KMS_RTP_SYNC_CONTEXT_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (                 \
    (obj),                                      \
    KMS_TYPE_RTP_SYNC_CONTEXT,                  \
    KmsRtpSyncContextPrivate                    \
  )                                             \
)

struct _KmsRtpSyncContextPrivate
{
  gsize initiated;

  /* Interstream synchronization */
  GstClockTime base_ntp_ns_time;
  GstClockTime base_sync_time;
};

static void
kms_rtp_sync_context_class_init (KmsRtpSyncContextClass * klass)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  g_type_class_add_private (klass, sizeof (KmsRtpSyncContextPrivate));
}

static void
kms_rtp_sync_context_init (KmsRtpSyncContext * self)
{
  self->priv = KMS_RTP_SYNC_CONTEXT_GET_PRIVATE (self);
}

KmsRtpSyncContext *
kms_rtp_sync_context_new ()
{
  KmsRtpSyncContext *self;

  self = KMS_RTP_SYNC_CONTEXT (g_object_new (KMS_TYPE_RTP_SYNC_CONTEXT, NULL));

  return self;
}

void
kms_rtp_sync_context_get_time_matching (KmsRtpSyncContext * self,
    GstClockTime ntp_ns_time_in, GstClockTime sync_time_in,
    GstClockTime * ntp_ns_time_out, GstClockTime * sync_time_out)
{
  if (g_once_init_enter (&self->priv->initiated)) {
    GST_DEBUG_OBJECT (self,
        "Setting base_ntp_ns_time: %" GST_TIME_FORMAT ", base_sync_time: %"
        GST_TIME_FORMAT, GST_TIME_ARGS (ntp_ns_time_in),
        GST_TIME_ARGS (sync_time_in));

    self->priv->base_ntp_ns_time = ntp_ns_time_in;
    self->priv->base_sync_time = sync_time_in;
    g_once_init_leave (&self->priv->initiated, 1);
  }

  *ntp_ns_time_out = self->priv->base_ntp_ns_time;
  *sync_time_out = self->priv->base_sync_time;
}
