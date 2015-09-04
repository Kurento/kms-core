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

#ifndef __KMS_DEC_TREE_BIN_H__
#define __KMS_DEC_TREE_BIN_H__

#include "kmstreebin.h"

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define KMS_TYPE_DEC_TREE_BIN \
  (kms_dec_tree_bin_get_type())
#define KMS_DEC_TREE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_DEC_TREE_BIN,KmsDecTreeBin))
#define KMS_DEC_TREE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_DEC_TREE_BIN,KmsDecTreeBinClass))
#define KMS_IS_DEC_TREE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_DEC_TREE_BIN))
#define KMS_IS_DEC_TREE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_DEC_TREE_BIN))
#define KMS_DEC_TREE_BIN_CAST(obj) ((KmsDecTreeBin*)(obj))

typedef struct _KmsDecTreeBin KmsDecTreeBin;
typedef struct _KmsDecTreeBinClass KmsDecTreeBinClass;

struct _KmsDecTreeBin
{
  KmsTreeBin parent;
};

struct _KmsDecTreeBinClass
{
  KmsTreeBinClass parent_class;
};

GType kms_dec_tree_bin_get_type (void);

KmsDecTreeBin * kms_dec_tree_bin_new (const GstCaps * caps, const GstCaps * raw_aps);

G_END_DECLS
#endif /* __KMS_DEC_TREE_BIN_H__ */
