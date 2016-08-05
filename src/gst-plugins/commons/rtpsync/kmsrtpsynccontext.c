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
#include <glib/gstdio.h>

#define GST_DEFAULT_NAME "rtpsynccontext"
GST_DEBUG_CATEGORY_STATIC (kms_rtp_sync_context_debug_category);
#define GST_CAT_DEFAULT kms_rtp_sync_context_debug_category

#define KMS_RTP_SYNC_STATS_PATH_ENV_VAR "KMS_RTP_SYNC_STATS_PATH"
static const gchar *stats_files_dir;

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

  FILE *stats_file;
  GMutex stats_mutex;
};

static void
kms_rtp_sync_context_finalize (GObject * object)
{
  KmsRtpSyncContext *self = KMS_RTP_SYNC_CONTEXT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  if (self->priv->stats_file) {
    fclose (self->priv->stats_file);
  }

  g_mutex_clear (&self->priv->stats_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_rtp_sync_context_class_init (KmsRtpSyncContextClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_rtp_sync_context_finalize;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  g_type_class_add_private (klass, sizeof (KmsRtpSyncContextPrivate));

  stats_files_dir = g_getenv (KMS_RTP_SYNC_STATS_PATH_ENV_VAR);
}

static void
kms_rtp_sync_context_init (KmsRtpSyncContext * self)
{
  self->priv = KMS_RTP_SYNC_CONTEXT_GET_PRIVATE (self);
  g_mutex_init (&self->priv->stats_mutex);
}

static void
kms_rtp_sync_context_init_stats_file (KmsRtpSyncContext * self,
    const gchar * stats_file_suffix_name)
{
  gchar *stats_file_name;
  GDateTime *datetime;
  gchar *date_str;

  if (stats_file_suffix_name == NULL) {
    return;
  }

  if (stats_files_dir == NULL) {
    return;
  }

  datetime = g_date_time_new_now_local ();
  date_str = g_date_time_format (datetime, "%Y%m%d%H%M%S");
  g_date_time_unref (datetime);

  stats_file_name =
      g_strdup_printf ("%s/%s_%s.csv", stats_files_dir, date_str,
      stats_file_suffix_name);
  g_free (date_str);

  if (g_mkdir_with_parents (stats_files_dir, 0777) < 0) {
    GST_ERROR_OBJECT (self,
        "Directory '%s' for stats files cannot be created", stats_files_dir);
    goto end;
  }

  self->priv->stats_file = g_fopen (stats_file_name, "w+");

  if (self->priv->stats_file == NULL) {
    GST_ERROR_OBJECT (self, "Stats file '%s' cannot be created",
        stats_file_name);
  } else {
    GST_INFO_OBJECT (self, "Stats file '%s' created", stats_file_name);
    g_fprintf (self->priv->stats_file,
        "ENTRY_TS,THREAD,SSRC,CLOCK_RATE,PTS_ORIG,PTS,DTS,EXT_RTP,SR_NTP_NS,SR_EXT_RTP\n");
  }

end:
  g_free (stats_file_name);
}

KmsRtpSyncContext *
kms_rtp_sync_context_new (const gchar * stats_file_suffix_name)
{
  KmsRtpSyncContext *self;

  self = KMS_RTP_SYNC_CONTEXT (g_object_new (KMS_TYPE_RTP_SYNC_CONTEXT, NULL));

  kms_rtp_sync_context_init_stats_file (self, stats_file_suffix_name);

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

gboolean
kms_rtp_sync_context_write_stats (KmsRtpSyncContext * self, guint32 ssrc,
    guint32 clock_rate, guint64 pts_orig, guint64 pts, guint64 dts,
    guint64 ext_ts, guint64 last_sr_ntp_ns_time, guint64 last_sr_ext_ts)
{
  if (self->priv->stats_file == NULL) {
    return FALSE;
  }

  g_mutex_lock (&self->priv->stats_mutex);
  g_fprintf (self->priv->stats_file,
      "%" G_GUINT64_FORMAT ",%p,%" G_GUINT32_FORMAT ",%" G_GUINT32_FORMAT ",%"
      G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT ",%"
      G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT ",%" G_GUINT64_FORMAT "\n",
      g_get_real_time (), g_thread_self (), ssrc, clock_rate, pts_orig,
      pts, dts, ext_ts, last_sr_ntp_ns_time, last_sr_ext_ts);
  g_mutex_unlock (&self->priv->stats_mutex);

  return TRUE;
}
