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
#include "kmsutils.h"

#include <gst/check/gstcheck.h>
#include <glib.h>

GST_START_TEST (check_urls)
{
  gchar *uri = "http://192.168.0.111:8080repository_servlet/video-upload";

  fail_if (kms_is_valid_uri (uri));

  uri = "http://192.168.0.111:8080/repository_servlet/video-upload";
  fail_if (!(kms_is_valid_uri (uri)));

  uri = "http://www.kurento.es/resource";
  fail_if (!(kms_is_valid_uri (uri)));

  uri = "http://localhost:8080/resource/res";
  fail_if (!(kms_is_valid_uri (uri)));

}

GST_END_TEST
/* Suite initialization */
static Suite *
utils_suite (void)
{
  Suite *s = suite_create ("utils");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_urls);

  return s;
}

GST_CHECK_MAIN (utils);
