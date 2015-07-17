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

/* Macros used to calculate latency stats */
#define KMS_STATS_ALPHA 0.25
#define KMS_STATS_CALCULATE_LATENCY_AVG(ti, ax) ({        \
  (ti) * KMS_STATS_ALPHA + (ax) * (1 - KMS_STATS_ALPHA);  \
})

typedef enum {
  KMS_STATS_ELEMENT,
  KMS_STATS_ENDPOINT
} KmsStatsType;

GstStructure * kms_stats_element_stats_new (KmsStatsType type, const gchar *id);
void kms_stats_add (GstStructure *stats, GstStructure *element_stats);
GstStructure * kms_stats_get_element_stats (GstStructure *stats);
void kms_stats_set_type (GstStructure *element_stats, KmsStatsType type);

/* buffer latency */
typedef void (*BufferLatencyCallback) (GstPad * pad, KmsMediaType type, GstClockTimeDiff t, gpointer user_data);
gulong kms_stats_add_buffer_latency_meta_probe (GstPad * pad, gboolean is_valid, KmsMediaType type);
gulong kms_stats_add_buffer_latency_notification_probe (GstPad * pad, BufferLatencyCallback cb, gpointer user_data, GDestroyNotify destroy_data);

typedef struct _KmsStatsProbe KmsStatsProbe;
typedef void (*StatsProbeCallback) (GstBuffer *buff, gpointer user_data);

KmsStatsProbe * kms_stats_probe_new (GstPad *pad);
void kms_stats_probe_destroy (KmsStatsProbe *probe);
void kms_stats_probe_add (KmsStatsProbe *probe, BufferLatencyCallback callback,
  gpointer user_data, GDestroyNotify destroy_data);
void kms_stats_probe_remove (KmsStatsProbe *probe);
gboolean kms_stats_probe_watches (KmsStatsProbe *probe, GstPad *pad);

#endif /* __KMS_STATS_H__ */
