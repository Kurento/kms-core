/*
 * kmscheck.h - gst-kurento-plugins
 *
 * Copyright (C) 2013 Kurento
 * Contact: Miguel París Díaz <mparisdiaz@gmail.com>
 * Contact: José Antonio Santos Cadenas <santoscadenas@kurento.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gst/check/gstcheck.h>

#define KMS_CHECK_MAIN(name)                                    \
int main (int argc, char **argv)                                \
{                                                               \
  Suite *s;                                                     \
  int ret;                                                      \
  gst_check_init (&argc, &argv);                                \
  s = name ## _suite ();                                        \
  ret = gst_check_run_suite (s, # name, __FILE__);              \
  gst_deinit ();                                                \
  return ret;                                                   \
}

#define KMS_END_TEST GST_LOG ("cleaning up tasks"); \
                     gst_task_cleanup_all (); \
                     gst_deinit (); \
                     END_TEST
