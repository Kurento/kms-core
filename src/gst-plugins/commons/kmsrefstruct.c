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

#include "kmsrefstruct.h"

void
kms_ref_struct_init (KmsRefStruct * refstruct, GDestroyNotify notif)
{
  g_return_if_fail (refstruct != NULL);

  refstruct->_count = 1;
  refstruct->_notif = notif;
}

KmsRefStruct *
kms_ref_struct_ref (KmsRefStruct * refstruct)
{
  g_return_val_if_fail (refstruct != NULL, NULL);

  g_atomic_int_inc (&refstruct->_count);

  return refstruct;
}

void
kms_ref_struct_unref (KmsRefStruct * refstruct)
{
  g_return_if_fail (refstruct != NULL);

  if (!g_atomic_int_dec_and_test (&refstruct->_count)) {
    return;
  }

  if (refstruct->_notif != NULL) {
    refstruct->_notif (refstruct);
  }
}
