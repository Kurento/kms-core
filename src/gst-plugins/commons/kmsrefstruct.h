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

#ifndef __KMS_REF_STRUCT_H__
#define __KMS_REF_STRUCT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _KmsRefStruct KmsRefStruct;
#define KMS_REF_STRUCT_CAST(obj) ((KmsRefStruct *) obj)

void kms_ref_struct_init (KmsRefStruct * refstruct, GDestroyNotify notif);
KmsRefStruct * kms_ref_struct_ref (KmsRefStruct * refstruct);
void kms_ref_struct_unref (KmsRefStruct * refstruct);

struct _KmsRefStruct {
  gint _count;
  GDestroyNotify _notif;
};

G_END_DECLS

#endif /* __KMS_REF_STRUCT_H__ */
