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

struct _KmsStatsProbe
{
  GstPad *pad;
  gulong probe_id;
};

typedef struct _BufferLatencyValues
{
  gboolean valid;
  KmsMediaType type;
} BufferLatencyValues;

typedef struct _ProbeData ProbeData;
typedef void (*BufferCb) (GstBuffer * buffer, ProbeData * pdata);

typedef struct _ProbeData
{
  BufferCb invoke_cb;
  gpointer invoke_data;
  GDestroyNotify destroy_invoke;

  GCallback cb;
  gpointer user_data;
  GDestroyNotify destroy_data;
} ProbeData;

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

static ProbeData *
probe_data_new (BufferCb invoke_cb, gpointer invoke_data,
    GDestroyNotify destroy_invoke, GCallback cb, gpointer user_data,
    GDestroyNotify destroy_data)
{
  ProbeData *pdata;

  pdata = g_slice_new (ProbeData);

  pdata->invoke_cb = invoke_cb;
  pdata->invoke_data = invoke_data;
  pdata->destroy_invoke = destroy_invoke;

  pdata->cb = cb;
  pdata->user_data = user_data;
  pdata->destroy_data = destroy_data;

  return pdata;
}

static void
probe_data_destroy (ProbeData * pdata)
{
  if (pdata->user_data != NULL && pdata->destroy_data != NULL) {
    pdata->destroy_data (pdata->user_data);
  }

  if (pdata->invoke_data != NULL && pdata->destroy_invoke != NULL) {
    pdata->destroy_invoke (pdata->invoke_data);
  }

  g_slice_free (ProbeData, pdata);
}

static void
process_buffer (GstBuffer * buffer, gpointer user_data)
{
  ProbeData *pdata = user_data;

  if (pdata->invoke_cb != NULL) {
    pdata->invoke_cb (buffer, pdata);
  }
}

static gboolean
process_buffer_list_cb (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  process_buffer (*buffer, user_data);

  return TRUE;
}

static GstPadProbeReturn
process_buffer_probe_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

    process_buffer (buffer, user_data);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);

    gst_buffer_list_foreach (list, process_buffer_list_cb, user_data);
  }

  return GST_PAD_PROBE_OK;
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
buffer_latency_probe_cb (GstBuffer * buffer, ProbeData * pdata)
{
  BufferLatencyValues *blv = (BufferLatencyValues *) pdata->invoke_data;
  GstClockTime time;

  time = kms_utils_get_time_nsecs ();

  kms_buffer_add_buffer_latency_meta (buffer, time, blv->valid, blv->type);
}

gulong
kms_stats_add_buffer_latency_meta_probe (GstPad * pad, gboolean is_valid,
    KmsMediaType type)
{
  ProbeData *pdata;

  BufferLatencyValues *blv;

  blv = buffer_latency_values_new (is_valid, type);

  pdata = probe_data_new (buffer_latency_probe_cb, blv,
      (GDestroyNotify) buffer_latency_values_destroy, NULL, NULL, NULL);

  return gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      process_buffer_probe_cb, pdata, (GDestroyNotify) probe_data_destroy);
}

static void
buffer_update_latency_probe_cb (GstBuffer * buffer, ProbeData * pdata)
{
  BufferLatencyValues *blv = (BufferLatencyValues *) pdata->invoke_data;
  KmsBufferLatencyMeta *meta;

  meta = kms_buffer_get_buffer_latency_meta (buffer);

  if (meta != NULL) {
    meta->type = blv->type;
    meta->valid = blv->valid;
  }
}

gulong
kms_stats_add_buffer_update_latency_meta_probe (GstPad * pad, gboolean is_valid,
    KmsMediaType type)
{
  ProbeData *pdata;

  BufferLatencyValues *blv;

  blv = buffer_latency_values_new (is_valid, type);

  pdata = probe_data_new (buffer_update_latency_probe_cb, blv,
      (GDestroyNotify) buffer_latency_values_destroy, NULL, NULL, NULL);

  return gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      process_buffer_probe_cb, pdata, (GDestroyNotify) probe_data_destroy);
}

static void
buffer_latency_calculation_cb (GstBuffer * buffer, ProbeData * pdata)
{
  BufferLatencyCallback func = (BufferLatencyCallback) pdata->cb;
  GstPad *pad = GST_PAD (pdata->invoke_data);
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

  if (func != NULL) {
    func (pad, meta->type, diff, pdata->user_data);
  }
}

gulong
kms_stats_add_buffer_latency_notification_probe (GstPad * pad,
    BufferLatencyCallback cb, gpointer user_data, GDestroyNotify destroy_data)
{
  ProbeData *pdata;

  pdata = probe_data_new (buffer_latency_calculation_cb, pad, NULL,
      G_CALLBACK (cb), user_data, destroy_data);

  return gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      process_buffer_probe_cb, pdata, (GDestroyNotify) probe_data_destroy);
}

KmsStatsProbe *
kms_stats_probe_new (GstPad * pad)
{
  KmsStatsProbe *probe;

  probe = g_slice_new0 (KmsStatsProbe);
  probe->pad = GST_PAD (g_object_ref (pad));

  return probe;
}

void
kms_stats_probe_destroy (KmsStatsProbe * probe)
{
  kms_stats_probe_remove (probe);

  g_object_unref (probe->pad);

  g_slice_free (KmsStatsProbe, probe);
}

void
kms_stats_probe_add (KmsStatsProbe * probe, BufferLatencyCallback callback,
    gpointer user_data, GDestroyNotify destroy_data)
{
  kms_stats_probe_remove (probe);

  probe->probe_id = kms_stats_add_buffer_latency_notification_probe (probe->pad,
      callback, user_data, destroy_data);
}

void
kms_stats_probe_remove (KmsStatsProbe * probe)
{
  if (probe->probe_id != 0UL) {
    gst_pad_remove_probe (probe->pad, probe->probe_id);
    probe->probe_id = 0UL;
  }
}

gboolean
kms_stats_probe_watches (KmsStatsProbe * probe, GstPad * pad)
{
  return pad == probe->pad;
}
