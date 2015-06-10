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

#include "kmsbufferlacentymeta.h"

GType
kms_buffer_latency_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("KmsBufferLatencyMetaAPI", tags);

    g_once_init_leave (&type, _type);
  }

  return type;
}

static gboolean
kms_buffer_latency_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  KmsBufferLatencyMeta *lmeta = (KmsBufferLatencyMeta *) meta;

  lmeta->ts = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static gboolean
kms_buffer_latency_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  KmsBufferLatencyMeta *lmeta;

  /* we always copy no matter what transform */
  if (GST_META_TRANSFORM_IS_COPY (type)) {
    lmeta = (KmsBufferLatencyMeta *) meta;

    GST_DEBUG ("copy latency metadata");
    kms_buffer_add_buffer_latency_meta (transbuf, lmeta->ts);
  }

  return TRUE;
}

static void
kms_buffer_latency_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  /* Nothing to do */
}

const GstMetaInfo *
kms_buffer_latency_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (KMS_BUFFER_LATENCY_META_API_TYPE,
        "KmsBufferLatencyMeta",
        sizeof (KmsBufferLatencyMeta),
        kms_buffer_latency_meta_init,
        kms_buffer_latency_meta_free,
        kms_buffer_latency_meta_transform);

    g_once_init_leave (&meta_info, mi);
  }

  return meta_info;
}

KmsBufferLatencyMeta *
kms_buffer_add_buffer_latency_meta (GstBuffer * buffer, GstClockTime ts)
{
  KmsBufferLatencyMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (KmsBufferLatencyMeta *) gst_buffer_add_meta (buffer,
      KMS_BUFFER_LATENCY_META_INFO, NULL);

  meta->ts = ts;

  return meta;
}
