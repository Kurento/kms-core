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

#include "kmsstats.h"
#include "kmsutils.h"
#include "kmsbufferlacentymeta.h"

#define KMS_MEDIA_ELEMENT_TAG "media-element"
#define KMS_ELEMENT_STATS_TAG "element-stats"

typedef struct _BufferLatencyData
{
  GstPad *pad;
  BufferLatencyCallback cb;
  gpointer user_data;
  GDestroyNotify destroy_data;
} BufferLatencyData;

typedef struct _BufferLatencyValues
{
  gboolean valid;
  KmsMediaType type;
} BufferLatencyValues;

static BufferLatencyValues *
buffer_latency_values_new (gboolean is_valid, KmsMediaType type)
{
  BufferLatencyValues *blv;

  blv = g_slice_new (BufferLatencyValues);

  blv->valid = is_valid;
  blv->type = type;

  return blv;
}

static void
buffer_latency_values_destroy (BufferLatencyValues * blv)
{
  g_slice_free (BufferLatencyValues, blv);
}

static BufferLatencyData *
buffer_latency_data_new (GstPad * pad, BufferLatencyCallback cb,
    gpointer user_data, GDestroyNotify destroy_data)
{
  BufferLatencyData *bl;

  bl = g_slice_new (BufferLatencyData);

  bl->pad = pad;
  bl->cb = cb;
  bl->user_data = user_data;
  bl->destroy_data = destroy_data;

  return bl;
}

static void
buffer_latency_data_destroy (BufferLatencyData * bl)
{
  if (bl->user_data != NULL && bl->destroy_data != NULL) {
    bl->destroy_data (bl->user_data);
  }

  g_slice_free (BufferLatencyData, bl);
}

static const gchar *
element_stats_type_to_string (KmsStatsType type)
{
  switch (type) {
    case KMS_STATS_ELEMENT:
      return "element";
    case KMS_STATS_ENDPOINT:
      return "endpoint";
    default:
      return NULL;
  }
}

GstStructure *
kms_stats_element_stats_new (KmsStatsType type, const gchar * id)
{
  return gst_structure_new (KMS_ELEMENT_STATS_TAG, "type", G_TYPE_STRING,
      element_stats_type_to_string (type), "id", G_TYPE_STRING, id, NULL);
}

void
kms_stats_add (GstStructure * stats, GstStructure * element_stats)
{
  gst_structure_set (stats, KMS_MEDIA_ELEMENT_TAG, GST_TYPE_STRUCTURE,
      element_stats, NULL);
}

GstStructure *
kms_stats_get_element_stats (GstStructure * stats)
{
  GstStructure *element_stats;
  const GValue *value;

  if (!gst_structure_has_field (stats, KMS_MEDIA_ELEMENT_TAG)) {
    return NULL;
  }

  value = gst_structure_get_value (stats, KMS_MEDIA_ELEMENT_TAG);

  if (!GST_VALUE_HOLDS_STRUCTURE (value)) {
    return NULL;
  }

  element_stats = (GstStructure *) gst_value_get_structure (value);

  if (g_strcmp0 (KMS_ELEMENT_STATS_TAG,
          gst_structure_get_name (element_stats)) != 0) {
    return NULL;
  }

  return element_stats;
}

void
kms_stats_set_type (GstStructure * element_stats, KmsStatsType type)
{
  gst_structure_set (element_stats, "type", G_TYPE_STRING,
      element_stats_type_to_string (type), NULL);
}

static void
add_buffer_latency_metadata (GstBuffer * buffer)
{
  GstClockTime time;

  time = kms_utils_get_time_nsecs ();

  kms_buffer_add_buffer_latency_meta (buffer, time, FALSE, 0);
}

static gboolean
add_buffer_list_latency_metadata (GstBuffer ** buffer, guint idx,
    gpointer user_data)
{
  add_buffer_latency_metadata (*buffer);

  return TRUE;
}

static GstPadProbeReturn
add_buffer_latency_metadata_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

    add_buffer_latency_metadata (buffer);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);

    gst_buffer_list_foreach (list, add_buffer_list_latency_metadata, NULL);
  }

  return GST_PAD_PROBE_OK;
}

static void
calculate_buffer_latency (GstBuffer * buffer, BufferLatencyData * bl)
{
  KmsBufferLatencyMeta *meta;
  GstClockTimeDiff diff;
  GstClockTime now;

  meta = kms_buffer_get_buffer_latency_meta (buffer);

  if (meta == NULL) {
    return;
  }

  if (!meta->valid) {
    /* Ignore this meta */
    return;
  }

  now = kms_utils_get_time_nsecs ();
  diff = GST_CLOCK_DIFF (meta->ts, now);

  if (bl->cb != NULL) {
    bl->cb (bl->pad, meta->type, diff, bl->user_data);
  }
}

static gboolean
calculate_buffer_list_latency (GstBuffer ** buffer, guint idx,
    gpointer user_data)
{
  calculate_buffer_latency (*buffer, user_data);

  return TRUE;
}

gulong
kms_stats_add_buffer_latency_meta_probe (GstPad * pad, gboolean is_valid,
    KmsMediaType type)
{
  BufferLatencyValues *blv;

  blv = buffer_latency_values_new (is_valid, type);

  return gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      add_buffer_latency_metadata_cb, blv,
      (GDestroyNotify) buffer_latency_values_destroy);
}

static GstPadProbeReturn
calculate_buffer_latency_metadata_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

    calculate_buffer_latency (buffer, user_data);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);

    gst_buffer_list_foreach (list, calculate_buffer_list_latency, user_data);
  }

  return GST_PAD_PROBE_OK;
}

gulong
kms_stats_add_buffer_latency_notification_probe (GstPad * pad,
    BufferLatencyCallback cb, gpointer user_data, GDestroyNotify destroy_data)
{
  BufferLatencyData *bl;

  bl = buffer_latency_data_new (pad, cb, user_data, destroy_data);

  return gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      calculate_buffer_latency_metadata_cb, bl,
      (GDestroyNotify) buffer_latency_data_destroy);
}
