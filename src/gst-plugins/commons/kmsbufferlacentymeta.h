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

#ifndef __KMS_BUFFER_LATENCY_META_H__
#define __KMS_BUFFER_LATENCY_META_H__

#include <gst/gst.h>

#include "kmsmediatype.h"
#include "kmslist.h"

G_BEGIN_DECLS

typedef struct _KmsBufferLatencyMeta KmsBufferLatencyMeta;

/**
 * KmsBufferLatencyMeta:
 * @meta: the parent type
 * @ts: The time stamp
 *
 * Buffer metadata for measuring buffer latency since the buffer is generated
 * until it is processed by a sink.
 */
struct _KmsBufferLatencyMeta {
  GstMeta       meta;

  GstClockTime ts;
  KmsMediaType type;
  gboolean valid;

  GRecMutex datamutex;
  KmsList *data; /* <string, refstruct> */
};

#define KMS_BUFFER_LATENCY_DATA_LOCK(mdata) \
  (g_rec_mutex_lock (&((KmsBufferLatencyMeta *)mdata)->datamutex))
#define KMS_BUFFER_LATENCY_DATA_UNLOCK(mdata) \
  (g_rec_mutex_unlock (&((KmsBufferLatencyMeta *)mdata)->datamutex))

GType kms_buffer_latency_meta_api_get_type (void);
#define KMS_BUFFER_LATENCY_META_API_TYPE \
  (kms_buffer_latency_meta_api_get_type())

#define kms_buffer_get_buffer_latency_meta(b) \
  ((KmsBufferLatencyMeta*)gst_buffer_get_meta((b), KMS_BUFFER_LATENCY_META_API_TYPE))

/* implementation */
const GstMetaInfo *kms_buffer_latency_meta_get_info (void);
#define KMS_BUFFER_LATENCY_META_INFO (kms_buffer_latency_meta_get_info ())

KmsBufferLatencyMeta * kms_buffer_add_buffer_latency_meta (GstBuffer *buffer,
  GstClockTime ts, gboolean valid, KmsMediaType type);

G_END_DECLS

#endif /* __KMS_BUFFER_LATENCY_META_H__ */
