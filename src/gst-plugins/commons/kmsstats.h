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
#ifndef __KMS_STATS_H__
#define __KMS_STATS_H__

#include "gst/gst.h"
#include "kmsmediatype.h"
#include "kmslist.h"

G_BEGIN_DECLS

#define KMS_MEDIA_ELEMENT_FIELD "media-element"
#define KMS_RTC_STATISTICS_FIELD "rtc-statistics"
#define KMS_DATA_SESSION_STATISTICS_FIELD "data-session-statistics"
#define KMS_ELEMENT_STATS_STRUCT_NAME "element-stats"
#define KMS_RTP_STRUCT_NAME "rtp-stats"
#define KMS_SESSIONS_STRUCT_NAME "session-stats"
#define KMS_DATA_SESSION_STRUCT_NAME "data-session-stats"

/* Macros used to calculate latency stats */
#define KMS_STATS_ALPHA 0.25
#define KMS_STATS_CALCULATE_LATENCY_AVG(ti, ax) ({        \
  (ti) * KMS_STATS_ALPHA + (ax) * (1 - KMS_STATS_ALPHA);  \
})

GstStructure * kms_stats_get_element_stats (GstStructure *stats);

/* buffer latency */
typedef void (*BufferLatencyCallback) (GstPad * pad, KmsMediaType type, GstClockTimeDiff t, KmsList *data, gpointer user_data);
gulong kms_stats_add_buffer_latency_meta_probe (GstPad * pad, gboolean is_valid, KmsMediaType type);
gulong kms_stats_add_buffer_update_latency_meta_probe (GstPad * pad, gboolean is_valid, KmsMediaType type);
gulong kms_stats_add_buffer_latency_notification_probe (GstPad * pad, BufferLatencyCallback cb, gboolean locked, gpointer user_data, GDestroyNotify destroy_data);

typedef struct _KmsStatsProbe KmsStatsProbe;

KmsStatsProbe * kms_stats_probe_new (GstPad *pad, KmsMediaType type);
void kms_stats_probe_destroy (KmsStatsProbe *probe);
void kms_stats_probe_add_latency (KmsStatsProbe *probe, BufferLatencyCallback callback,
  gboolean locked, gpointer user_data, GDestroyNotify destroy_data);
void kms_stats_probe_latency_meta_set_valid (KmsStatsProbe *probe, gboolean is_valid);
void kms_stats_probe_remove (KmsStatsProbe *probe);
gboolean kms_stats_probe_watches (KmsStatsProbe *probe, GstPad *pad);

G_END_DECLS

#endif /* __KMS_STATS_H__ */
