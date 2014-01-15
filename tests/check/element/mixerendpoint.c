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
#include <gst/gst.h>
#include <glib.h>

GST_START_TEST (create_element)
{
  GstElement *mixerendpoint;

  mixerendpoint = gst_element_factory_make ("mixerendpoint", NULL);

  fail_unless (mixerendpoint != NULL);

  g_object_unref (mixerendpoint);
}

GST_END_TEST static Suite *
mixerendpoint_suite (void)
{
  Suite *s = suite_create ("mixerendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, create_element);

  return s;
}

GST_CHECK_MAIN (mixerendpoint);
