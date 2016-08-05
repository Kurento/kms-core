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

#ifndef __KMS_RTP_SYNC_CONTEXT_H__
#define __KMS_RTP_SYNC_CONTEXT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_RTP_SYNC_CONTEXT \
  (kms_rtp_sync_context_get_type())

#define KMS_RTP_SYNC_CONTEXT(obj) ( \
  G_TYPE_CHECK_INSTANCE_CAST (      \
    (obj),                          \
    KMS_TYPE_RTP_SYNC_CONTEXT,      \
    KmsRtpSyncContext               \
  )                                 \
)
#define KMS_RTP_SYNC_CONTEXT_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (                 \
    (klass),                                \
    KMS_TYPE_RTP_SYNC_CONTEXT,              \
    KmsRtpSyncContextClass                  \
  )                                         \
)
#define KMS_IS_RTP_SYNC_CONTEXT(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (         \
    (obj),                             \
    KMS_TYPE_RTP_SYNC_CONTEXT          \
  )                                    \
)
#define KMS_IS_RTP_SYNC_CONTEXT_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_TYPE (                    \
    (klass),                                   \
    KMS_TYPE_RTP_SYNC_CONTEXT                  \
  )                                            \

#define KMS_RTP_SYNC_CONTEXT_GET_CLASS(obj) ( \
  G_TYPE_INSTANCE_GET_CLASS (                 \
    (obj),                                    \
    KMS_TYPE_RTP_SYNC_CONTEXT,                \
    KmsRtpSyncContextClass                    \
  )                                           \
)

#define KMS_RTP_SYNC_CONTEXT_CAST(obj) ((KmsRtpSyncContext*)(obj))

typedef struct _KmsRtpSyncContext KmsRtpSyncContext;
typedef struct _KmsRtpSyncContextClass KmsRtpSyncContextClass;
typedef struct _KmsRtpSyncContextPrivate KmsRtpSyncContextPrivate;

struct _KmsRtpSyncContext
{
  GObject parent;

  KmsRtpSyncContextPrivate *priv;
};

struct _KmsRtpSyncContextClass
{
  GObjectClass parent_class;
};

GType kms_rtp_sync_context_get_type ();

KmsRtpSyncContext * kms_rtp_sync_context_new (const gchar * stats_file_suffix_name);
void kms_rtp_sync_context_get_time_matching (KmsRtpSyncContext * self,
                                             GstClockTime ntp_ns_time_in,
                                             GstClockTime sync_time_in,
                                             GstClockTime * ntp_ns_time_out,
                                             GstClockTime * sync_time_out);

gboolean kms_rtp_sync_context_write_stats (KmsRtpSyncContext * self,
                                           guint32 ssrc,
                                           guint32 clock_rate,
                                           guint64 pts_orig,
                                           guint64 pts,
                                           guint64 dts,
                                           guint64 ext_ts,
                                           guint64 last_sr_ntp_ns_time,
                                           guint64 last_sr_ext_ts);

G_END_DECLS

#endif /* __KMS_RTP_SYNC_CONTEXT_H__ */
