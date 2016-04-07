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

#include "kmsserializablemeta.h"

GType
kms_serializable_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("KmsSerializableMetaAPI", tags);

    g_once_init_leave (&type, _type);
  }

  return type;
}

static gboolean
kms_serializable_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  KmsSerializableMeta *smeta = (KmsSerializableMeta *) meta;

  smeta->data = NULL;

  return TRUE;
}

static gboolean
kms_serializable_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  KmsSerializableMeta *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstStructure *copy = NULL;

    smeta = (KmsSerializableMeta *) meta;

    if (smeta->data != NULL) {
      copy = gst_structure_copy (smeta->data);
    }

    GST_DEBUG ("copy serializable metadata");
    kms_buffer_add_serializable_meta (transbuf, copy);
  }

  return TRUE;
}

static void
kms_serializable_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  KmsSerializableMeta *smeta = (KmsSerializableMeta *) meta;

  if (smeta->data != NULL) {
    gst_structure_free (smeta->data);
  }
}

const GstMetaInfo *
kms_serializable_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (KMS_SERIALIZABLE_META_API_TYPE,
        "KmsSerializableMeta",
        sizeof (KmsSerializableMeta),
        kms_serializable_meta_init,
        kms_serializable_meta_free,
        kms_serializable_meta_transform);

    g_once_init_leave (&meta_info, mi);
  }

  return meta_info;
}

gboolean
add_fields_to_structure (GQuark field_id, const GValue * value, gpointer st)
{
  GstStructure *data = GST_STRUCTURE (st);

  gst_structure_id_set_value (data, field_id, value);

  return TRUE;
}

KmsSerializableMeta *
kms_buffer_add_serializable_meta (GstBuffer * buffer, GstStructure * data)
{
  KmsSerializableMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = (KmsSerializableMeta *) gst_buffer_get_meta (buffer,
      KMS_SERIALIZABLE_META_API_TYPE);

  if (meta != NULL) {
    gst_structure_foreach (data, add_fields_to_structure, meta->data);
    gst_structure_free (data);

  } else {
    meta = (KmsSerializableMeta *) gst_buffer_add_meta (buffer,
        KMS_SERIALIZABLE_META_INFO, NULL);

    meta->data = data;
  }

  return meta;
}
