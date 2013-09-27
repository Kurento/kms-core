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

static void
setup (void)
{
  GST_INFO ("Do some startup here");
}

static void
teardown (void)
{
  GST_INFO ("Do cleaup here");
}

GST_START_TEST (dummy1)
{
  GST_INFO ("Perform test1 here");
  fail_if (FALSE);
}

GST_END_TEST
GST_START_TEST (dummy2)
{
  GST_INFO ("Perform test2 here");
  fail_if (FALSE);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
dummy_suite (void)
{
  Suite *s = suite_create ("dummy");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, dummy1);
  tcase_add_test (tc_chain, dummy2);

  return s;
}

GST_CHECK_MAIN (dummy);
