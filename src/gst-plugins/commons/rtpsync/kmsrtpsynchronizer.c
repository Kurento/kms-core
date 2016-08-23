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

#include "kmsrtpsynchronizer.h"

#define GST_DEFAULT_NAME "rtpsynchronizer"
GST_DEBUG_CATEGORY_STATIC (kms_rtp_synchronizer_debug_category);
#define GST_CAT_DEFAULT kms_rtp_synchronizer_debug_category

#define parent_class kms_rtp_synchronizer_parent_class
G_DEFINE_TYPE (KmsRtpSynchronizer, kms_rtp_synchronizer, G_TYPE_OBJECT);

#define KMS_RTP_SYNCHRONIZER_LOCK(rtpsynchronizer) \
  (g_rec_mutex_lock (&KMS_RTP_SYNCHRONIZER_CAST ((rtpsynchronizer))->priv->mutex))
#define KMS_RTP_SYNCHRONIZER_UNLOCK(rtpsynchronizer) \
  (g_rec_mutex_unlock (&KMS_RTP_SYNCHRONIZER_CAST ((rtpsynchronizer))->priv->mutex))

#define KMS_RTP_SYNCHRONIZER_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (                 \
    (obj),                                      \
    KMS_TYPE_RTP_SYNCHRONIZER,                  \
    KmsRtpSynchronizerPrivate                   \
  )                                             \
)

struct _KmsRtpSynchronizerPrivate
{
  GRecMutex mutex;

  KmsRtpSyncContext *context;

  guint32 ssrc;
  GHashTable *clock_rates;      /* <pt, clock_rate> */

  gboolean base_initiated;
  GstClockTime base_ntp_ns_time;
  GstClockTime base_sync_time;

  guint64 ext_ts;
  guint64 last_sr_ext_ts;
  guint64 last_sr_ntp_ns_time;
};

static void
kms_rtp_synchronizer_finalize (GObject * object)
{
  KmsRtpSynchronizer *self = KMS_RTP_SYNCHRONIZER (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_hash_table_destroy (self->priv->clock_rates);
  g_object_unref (self->priv->context);
  g_rec_mutex_clear (&self->priv->mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_rtp_synchronizer_class_init (KmsRtpSynchronizerClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_rtp_synchronizer_finalize;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  g_type_class_add_private (klass, sizeof (KmsRtpSynchronizerPrivate));
}

static void
kms_rtp_synchronizer_init (KmsRtpSynchronizer * self)
{
  self->priv = KMS_RTP_SYNCHRONIZER_GET_PRIVATE (self);
  g_rec_mutex_init (&self->priv->mutex);
  self->priv->clock_rates = g_hash_table_new (NULL, NULL);

  self->priv->ext_ts = -1;
}

KmsRtpSynchronizer *
kms_rtp_synchronizer_new (KmsRtpSyncContext * context)
{
  KmsRtpSynchronizer *self;

  self = KMS_RTP_SYNCHRONIZER (g_object_new (KMS_TYPE_RTP_SYNCHRONIZER, NULL));

  if (context != NULL) {
    self->priv->context = g_object_ref (context);
  } else {
    GST_WARNING_OBJECT (self,
        "No context provided, creating new one. This syncrhonizer cannot be synced with others.");
    self->priv->context = kms_rtp_sync_context_new ();
  }

  return self;
}

gboolean
kms_rtp_synchronizer_add_clock_rate_for_pt (KmsRtpSynchronizer * self,
    gint32 pt, gint32 clock_rate, GError ** error)
{
  gpointer ptr;
  gboolean ret = FALSE;

  if (clock_rate <= 0) {
    const gchar *msg = "clock-rate <= 0 no allowed.";

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR, KMS_RTP_SYNC_INVALID_DATA,
        msg);

    return FALSE;
  }

  KMS_RTP_SYNCHRONIZER_LOCK (self);

  /* TODO: allow more than one PT */
  if (g_hash_table_size (self->priv->clock_rates) > 0) {
    const gchar *msg = "Only one PT allowed.";

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR, KMS_RTP_SYNC_INVALID_DATA,
        msg);

    goto end;
  }

  ptr = g_hash_table_lookup (self->priv->clock_rates, GUINT_TO_POINTER (pt));
  if (ptr != NULL) {
    gchar *msg =
        g_strdup_printf ("There is already a clock-rate (%u) for PT (%u).",
        GPOINTER_TO_UINT (ptr), pt);

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR, KMS_RTP_SYNC_INVALID_DATA,
        msg);
    g_free (msg);

    goto end;
  }

  g_hash_table_insert (self->priv->clock_rates,
      GUINT_TO_POINTER (pt), GINT_TO_POINTER (clock_rate));

  ret = TRUE;

end:
  KMS_RTP_SYNCHRONIZER_UNLOCK (self);

  return ret;
}

static void
kms_rtp_synchronizer_process_rtcp_packet (KmsRtpSynchronizer * self,
    GstRTCPPacket * packet, GstClockTime current_time)
{
  GstRTCPType type;
  guint32 ssrc, rtp_time;
  guint64 ntp_time, ntp_ns_time;

  type = gst_rtcp_packet_get_type (packet);
  GST_DEBUG_OBJECT (self, "Received RTCP buffer of type: %d", type);

  if (type != GST_RTCP_TYPE_SR) {
    return;
  }

  gst_rtcp_packet_sr_get_sender_info (packet, &ssrc, &ntp_time, &rtp_time,
      NULL, NULL);

  /* convert ntp_time to nanoseconds */
  ntp_ns_time =
      gst_util_uint64_scale (ntp_time, GST_SECOND,
      (G_GINT64_CONSTANT (1) << 32));

  KMS_RTP_SYNCHRONIZER_LOCK (self);

  GST_DEBUG_OBJECT (self,
      "Received RTCP SR packet SSRC: %u, rtp_time: %u, ntp_time: %lu, ntp_ns_time: %"
      GST_TIME_FORMAT, ssrc, rtp_time, ntp_time, GST_TIME_ARGS (ntp_ns_time));

  if (!self->priv->base_initiated) {
    kms_rtp_sync_context_get_time_matching (self->priv->context, ntp_ns_time,
        current_time, &self->priv->base_ntp_ns_time,
        &self->priv->base_sync_time);
    self->priv->base_initiated = TRUE;
  }

  self->priv->last_sr_ext_ts =
      gst_rtp_buffer_ext_timestamp (&self->priv->ext_ts, rtp_time);
  self->priv->last_sr_ntp_ns_time = ntp_ns_time;

  KMS_RTP_SYNCHRONIZER_UNLOCK (self);
}

gboolean
kms_rtp_synchronizer_process_rtcp_buffer (KmsRtpSynchronizer * self,
    GstBuffer * buffer, GstClockTime current_time, GError ** error)
{
  GstRTCPBuffer rtcp_buffer = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;

  if (!gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp_buffer)) {
    const gchar *msg = "Buffer cannot be mapped as RTCP";

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR,
        KMS_RTP_SYNC_UNEXPECTED_ERROR, msg);

    return FALSE;
  }

  if (!gst_rtcp_buffer_get_first_packet (&rtcp_buffer, &packet)) {
    GST_WARNING_OBJECT (self, "Empty RTCP buffer");
    goto unmap;
  }

  kms_rtp_synchronizer_process_rtcp_packet (self, &packet, current_time);

unmap:
  gst_rtcp_buffer_unmap (&rtcp_buffer);

  return TRUE;
}

gboolean
kms_rtp_synchronizer_process_rtp_buffer_mapped (KmsRtpSynchronizer * self,
    GstRTPBuffer * rtp_buffer, GError ** error)
{
  GstBuffer *buffer = rtp_buffer->buffer;
  guint64 diff_ntp_ns_time, diff_rtptime, diff_rtp_ns_time;
  guint8 pt;
  guint32 ssrc, ts;
  gint32 clock_rate;

  ssrc = gst_rtp_buffer_get_ssrc (rtp_buffer);

  KMS_RTP_SYNCHRONIZER_LOCK (self);

  if (self->priv->ssrc == 0) {
    self->priv->ssrc = ssrc;
  } else if (ssrc != self->priv->ssrc) {
    gchar *msg = g_strdup_printf ("Invalid SSRC (%u), not matching with %u",
        ssrc, self->priv->ssrc);

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR, KMS_RTP_SYNC_INVALID_DATA,
        msg);
    g_free (msg);

    KMS_RTP_SYNCHRONIZER_UNLOCK (self);

    return FALSE;
  }

  pt = gst_rtp_buffer_get_payload_type (rtp_buffer);
  clock_rate =
      GPOINTER_TO_INT (g_hash_table_lookup (self->priv->clock_rates,
          GUINT_TO_POINTER (pt)));
  if (clock_rate <= 0) {
    gchar *msg =
        g_strdup_printf ("Invalid clock-rate for PT %u, not changing PTS",
        pt);

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR, KMS_RTP_SYNC_INVALID_DATA,
        msg);
    g_free (msg);

    KMS_RTP_SYNCHRONIZER_UNLOCK (self);

    return FALSE;
  }

  if (!self->priv->base_initiated) {
    GST_DEBUG_OBJECT (self, "No sync data, not changing PTS");
    KMS_RTP_SYNCHRONIZER_UNLOCK (self);

    return TRUE;
  }

  buffer = gst_buffer_make_writable (buffer);
  GST_BUFFER_PTS (buffer) = self->priv->base_sync_time;

  if (self->priv->last_sr_ntp_ns_time > self->priv->base_ntp_ns_time) {
    diff_ntp_ns_time =
        self->priv->last_sr_ntp_ns_time - self->priv->base_ntp_ns_time;
    GST_BUFFER_PTS (buffer) += diff_ntp_ns_time;
  } else if (self->priv->last_sr_ntp_ns_time < self->priv->base_ntp_ns_time) {
    diff_ntp_ns_time =
        self->priv->base_ntp_ns_time - self->priv->last_sr_ntp_ns_time;
    GST_BUFFER_PTS (buffer) -= diff_ntp_ns_time;
  }
  /* if equals do nothing */

  ts = gst_rtp_buffer_get_timestamp (rtp_buffer);
  gst_rtp_buffer_ext_timestamp (&self->priv->ext_ts, ts);

  if (self->priv->ext_ts > self->priv->last_sr_ext_ts) {
    diff_rtptime = self->priv->ext_ts - self->priv->last_sr_ext_ts;
    diff_rtp_ns_time =
        gst_util_uint64_scale_int (diff_rtptime, GST_SECOND, clock_rate);
    GST_BUFFER_PTS (buffer) += diff_rtp_ns_time;
  } else if (self->priv->ext_ts < self->priv->last_sr_ext_ts) {
    diff_rtptime = self->priv->last_sr_ext_ts - self->priv->ext_ts;
    diff_rtp_ns_time =
        gst_util_uint64_scale_int (diff_rtptime, GST_SECOND, clock_rate);
    GST_BUFFER_PTS (buffer) -= diff_rtp_ns_time;
  }
  /* if equals do nothing */

  KMS_RTP_SYNCHRONIZER_UNLOCK (self);

  return TRUE;
}

gboolean
kms_rtp_synchronizer_process_rtp_buffer (KmsRtpSynchronizer * self,
    GstBuffer * buffer, GError ** error)
{
  GstRTPBuffer rtp_buffer = GST_RTP_BUFFER_INIT;
  gboolean ret;

  if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp_buffer)) {
    const gchar *msg = "Buffer cannot be mapped as RTP";

    GST_ERROR_OBJECT (self, "%s", msg);
    g_set_error_literal (error, KMS_RTP_SYNC_ERROR,
        KMS_RTP_SYNC_UNEXPECTED_ERROR, msg);
    return FALSE;
  }

  ret =
      kms_rtp_synchronizer_process_rtp_buffer_mapped (self, &rtp_buffer, error);

  gst_rtp_buffer_unmap (&rtp_buffer);

  return ret;
}
