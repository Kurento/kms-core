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

#include <gst/check/gstcheck.h>
#include <glib.h>

#include "kmsrefstruct.h"

static gboolean is_freed = FALSE;

typedef struct _MyRefStruct
{
  KmsRefStruct ref;
  gint val;
} MyRefStruct;

static void
my_ref_struct_destroy (MyRefStruct * data)
{
  GST_DEBUG ("Free data");

  g_slice_free (MyRefStruct, data);

  /* Destroy function has been executed */
  is_freed = TRUE;
}

GST_START_TEST (check_refs)
{
  MyRefStruct *refs;

  refs = g_slice_new0 (MyRefStruct);
  kms_ref_struct_init (KMS_REF_STRUCT_CAST (refs),
      (GDestroyNotify) my_ref_struct_destroy);

  kms_ref_struct_ref (KMS_REF_STRUCT_CAST (refs));
  fail_if (is_freed);
  kms_ref_struct_ref (KMS_REF_STRUCT_CAST (refs));
  fail_if (is_freed);

  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (refs));
  fail_if (is_freed);
  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (refs));
  fail_if (is_freed);

  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (refs));

  /* Data should have been released */
  fail_unless (is_freed);
}

GST_END_TEST
/* Suite initialization */
static Suite *
refcount_suite (void)
{
  Suite *s = suite_create ("common");
  TCase *tc_chain = tcase_create ("refstruct");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_refs);

  return s;
}

GST_CHECK_MAIN (refcount);
