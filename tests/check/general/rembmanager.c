/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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

static void
bitrate_cb (RembEventManager * manager, guint bitrate, gpointer user_data)
{
  guint *min_br = (guint *) user_data;

  *min_br = bitrate;
}

GST_START_TEST (check_min_br_update)
{
  GstPad *pad;
  RembEventManager *manager;
  GstEvent *event;
  guint min_br;

  pad = gst_pad_new (NULL, GST_PAD_SRC);
  gst_pad_set_active (pad, TRUE);
  manager = kms_utils_remb_event_manager_create (pad);
  kms_utils_remb_event_manager_set_callback (manager, bitrate_cb, &min_br,
      NULL);

  /* SSRC_1: set min */
  event = kms_utils_remb_event_upstream_new (100, 1);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 100);

  /* SSRC_1: update min */
  event = kms_utils_remb_event_upstream_new (200, 1);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 200);

  /* SSRC_2: not update min */
  event = kms_utils_remb_event_upstream_new (300, 2);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 200);

  /* SSRC_2: update min */
  event = kms_utils_remb_event_upstream_new (100, 2);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 100);

  /* SSRC_1: not update min */
  event = kms_utils_remb_event_upstream_new (200, 1);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 100);

  /* SSRC_2: min is the last SSRC_1 br */
  event = kms_utils_remb_event_upstream_new (300, 2);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 200);

  /* SSRC_2: not update min */
  event = kms_utils_remb_event_upstream_new (300, 2);
  gst_pad_send_event (pad, event);
  fail_unless (min_br == 200);

  kms_utils_remb_event_manager_destroy (manager);
  g_object_unref (pad);
}

GST_END_TEST;

/* Suite initialization */
static Suite *
rembmanager_suite (void)
{
  Suite *s = suite_create ("rembmanager");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_min_br_update);

  return s;
}

GST_CHECK_MAIN (rembmanager);
