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

#ifndef __KMS_SERIALIZABLE_META_H__
#define __KMS_SERIALIZABLE_META_H__

#include <gst/gst.h>

#include "kmsmediatype.h"

G_BEGIN_DECLS

typedef struct _KmsSerializableMeta KmsSerializableMeta;

/**
 * KmsSerializableMeta:
 * @meta: the parent type
 * @ts: The time stamp
 *
 * Metadata for sending aditional information that can be passed over network
 * with the buffer
 */
struct _KmsSerializableMeta {
  GstMeta       meta;

  GstStructure *data;
};

GType kms_serializable_meta_api_get_type (void);
#define KMS_SERIALIZABLE_META_API_TYPE \
  (kms_serializable_meta_api_get_type())

#define kms_buffer_get_serializable_meta(b) \
  ((KmsSerializableMeta*)gst_buffer_get_meta((b), KMS_SERIALIZABLE_META_API_TYPE))

/* implementation */
const GstMetaInfo *kms_serializable_meta_get_info (void);
#define KMS_SERIALIZABLE_META_INFO (kms_serializable_meta_get_info ())


/**
 * kms_buffer_add_serializable_meta
 *
 * If the buffer doesn't contains any metadata of type KMS_SERIALIZABLE_META_INFO,
 * the data will be inserted, else the data contained in the buffer is merged
 * with the new data passed by the user. In case of collision, the old data
 * will be overwritten.
 *
 * @param buffer: the buffer where add the metadata
 * @param data: the structure to add as metadata
 * @return The metadata inserted in the buffer
 */
KmsSerializableMeta * kms_buffer_add_serializable_meta (GstBuffer *buffer,
  GstStructure *data);

/**
 * kms_serializable_meta_get_metadata
 *
 * This function returns the metadata into a buffer. The metadata has the same
 * life cycle than the type which contains it in the buffer.
 *
 * @param b: the buffer which contains the metadata
 * @return The metadata [transfer none]
 */
GstStructure * kms_serializable_meta_get_metadata (GstBuffer *buffer);

G_END_DECLS

#endif /* __KMS_SERIALIZABLE_META_H__ */
