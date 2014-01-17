/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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
#include "kmsbasemixer.h"

GST_START_TEST (create)
{
  KmsBaseMixer *mixer = g_object_new (KMS_TYPE_BASE_MIXER, NULL);

  fail_unless (mixer != NULL);

  g_object_unref (mixer);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
base_mixer_suite (void)
{
  Suite *s = suite_create ("basemixer");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create);

  return s;
}

GST_CHECK_MAIN (base_mixer);
