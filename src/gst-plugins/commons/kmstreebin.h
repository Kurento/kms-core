/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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

#ifndef __KMS_TREE_BIN_H__
#define __KMS_TREE_BIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define KMS_TYPE_TREE_BIN \
  (kms_tree_bin_get_type())
#define KMS_TREE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_TREE_BIN,KmsTreeBin))
#define KMS_TREE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_TREE_BIN,KmsTreeBinClass))
#define KMS_IS_TREE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_TREE_BIN))
#define KMS_IS_TREE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_TREE_BIN))
#define KMS_TREE_BIN_CAST(obj) ((KmsTreeBin*)(obj))

typedef struct _KmsTreeBin KmsTreeBin;
typedef struct _KmsTreeBinClass KmsTreeBinClass;
typedef struct _KmsTreeBinPrivate KmsTreeBinPrivate;

struct _KmsTreeBin
{
  GstBin parent;

  KmsTreeBinPrivate *priv;
};

struct _KmsTreeBinClass
{
  GstBinClass parent_class;
};

GType kms_tree_bin_get_type (void);

GstElement * kms_tree_bin_get_input_element (KmsTreeBin * self);
void kms_tree_bin_set_input_element (KmsTreeBin * self,
    GstElement * input_element);
GstElement * kms_tree_bin_get_output_tee (KmsTreeBin * self);

void kms_tree_bin_unlink_input_element_from_tee (KmsTreeBin * self);

GstCaps * kms_tree_bin_get_input_caps (KmsTreeBin *self);

G_END_DECLS
#endif /* __KMS_TREE_BIN_H__ */
