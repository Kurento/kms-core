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
#include "kmsagnosticcaps.h"
#include <gst/video/video-event.h>
#include "kmsagnosticcaps.h"

#define GST_CAT_DEFAULT kmsutils
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmsutils"
#define LAST_KEY_FRAME_REQUEST_TIME "kmslastkeyframe"

#define DEFAULT_KEYFRAME_DISPERSION 300000000   /* 300 ms */

void
kms_utils_set_valve_drop (GstElement * valve, gboolean drop)
{
  gboolean old_drop;

  g_object_get (valve, "drop", &old_drop, NULL);
  if (drop == old_drop)
    return;

  g_object_set (valve, "drop", drop, NULL);
}

static gboolean
debug_graph (gpointer bin)
{
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (bin),
      GST_DEBUG_GRAPH_SHOW_ALL, GST_ELEMENT_NAME (bin));
  return FALSE;
}

void
kms_utils_debug_graph_delay (GstBin * bin, guint interval)
{
  g_timeout_add_seconds (interval, debug_graph, bin);
}

gboolean
kms_is_valid_uri (const gchar * url)
{
  gboolean ret;
  GRegex *regex;

  regex = g_regex_new ("^(?:((?:https?):)\\/\\/)([^:\\/\\s]+)(?::(\\d*))?(?:\\/"
      "([^\\s?#]+)?([?][^?#]*)?(#.*)?)?$", 0, 0, NULL);
  ret = g_regex_match (regex, url, G_REGEX_MATCH_ANCHORED, NULL);
  g_regex_unref (regex);

  return ret;
}

gboolean
gst_element_sync_state_with_parent_target_state (GstElement * element)
{
  GstElement *parent;
  GstState target;
  GstStateChangeReturn ret;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  parent = GST_ELEMENT_CAST (gst_element_get_parent (element));

  if (parent == NULL) {
    GST_DEBUG_OBJECT (element, "element has no parent");
    return FALSE;
  }

  GST_OBJECT_LOCK (parent);
  target = GST_STATE_TARGET (parent);
  GST_OBJECT_UNLOCK (parent);

  GST_DEBUG_OBJECT (element,
      "setting parent (%s) target state %s",
      GST_ELEMENT_NAME (parent), gst_element_state_get_name (target));

  gst_object_unref (parent);

  ret = gst_element_set_state (element, target);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_DEBUG_OBJECT (element,
        "setting target state failed (%s)",
        gst_element_state_change_return_get_name (ret));

    return FALSE;
  }

  return TRUE;
}

/* Caps begin */

static GstStaticCaps static_audio_caps =
GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS);
static GstStaticCaps static_video_caps =
GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS);

static gboolean
caps_can_intersect_with_static (const GstCaps * caps,
    GstStaticCaps * static_caps)
{
  GstCaps *aux;
  gboolean ret;

  if (caps == NULL) {
    return FALSE;
  }

  aux = gst_static_caps_get (static_caps);
  ret = gst_caps_can_intersect (caps, aux);
  gst_caps_unref (aux);

  return ret;
}

gboolean
kms_utils_caps_are_audio (const GstCaps * caps)
{
  return caps_can_intersect_with_static (caps, &static_audio_caps);
}

gboolean
kms_utils_caps_are_video (const GstCaps * caps)
{
  return caps_can_intersect_with_static (caps, &static_video_caps);
}

/* Caps end */

GstElement *
kms_utils_create_convert_for_caps (const GstCaps * caps)
{
  if (kms_utils_caps_are_audio (caps)) {
    return gst_element_factory_make ("audioconvert", NULL);
  } else {
    return gst_element_factory_make ("videoconvert", NULL);
  }
}

GstElement *
kms_utils_create_mediator_element (const GstCaps * caps)
{
  if (kms_utils_caps_are_audio (caps)) {
    return gst_element_factory_make ("audioresample", NULL);
  } else {
    return gst_element_factory_make ("videoscale", NULL);
  }
}

GstElement *
kms_utils_create_rate_for_caps (const GstCaps * caps)
{
  GstElement *rate;

  if (kms_utils_caps_are_audio (caps)) {
    rate = gst_element_factory_make ("audiorate", NULL);
    g_object_set (G_OBJECT (rate), "tolerance", GST_MSECOND * 100,
        "skip-to-first", TRUE, NULL);
  } else {
    rate = gst_element_factory_make ("videorate", NULL);
    g_object_set (G_OBJECT (rate), "average-period", GST_MSECOND * 200,
        "skip-to-first", TRUE, NULL);
  }

  return rate;
}

/* key frame management */

#define DROPPING_UNTIL_KEY_FRAME "dropping_until_key_frame"

/* Call this function holding the lock */
static inline gboolean
is_dropping (GstPad * pad)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pad),
          DROPPING_UNTIL_KEY_FRAME));
}

/* Call this function holding the lock */
static inline void
set_dropping (GstPad * pad, gboolean dropping)
{
  g_object_set_data (G_OBJECT (pad), DROPPING_UNTIL_KEY_FRAME,
      GINT_TO_POINTER (dropping));
}

static gboolean
is_raw_caps (GstCaps * caps)
{
  gboolean ret;
  GstCaps *raw_caps = gst_caps_from_string (KMS_AGNOSTIC_RAW_CAPS);

  ret = gst_caps_is_always_compatible (caps, raw_caps);

  gst_caps_unref (raw_caps);
  return ret;
}

static void
send_force_key_unit_event (GstPad * pad, gboolean all_headers)
{
  GstEvent *event;
  GstCaps *caps = gst_pad_get_current_caps (pad);

  if (caps == NULL) {
    caps = gst_pad_get_allowed_caps (pad);
  }

  if (caps == NULL) {
    return;
  }

  if (is_raw_caps (caps)) {
    goto end;
  }

  event =
      gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
      all_headers, 0);

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SRC) {
    gst_pad_send_event (pad, event);
  } else {
    gst_pad_push_event (pad, event);
  }

end:
  gst_caps_unref (caps);
}

static GstPadProbeReturn
drop_until_keyframe_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstBuffer *buffer;
  gboolean all_headers = GPOINTER_TO_INT (user_data);

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
    /* Drop buffer until a keyframe is received */
    send_force_key_unit_event (pad, all_headers);
    GST_TRACE_OBJECT (pad, "Dropping buffer");
    return GST_PAD_PROBE_DROP;
  }

  GST_OBJECT_LOCK (pad);
  set_dropping (pad, FALSE);
  GST_OBJECT_UNLOCK (pad);

  GST_DEBUG_OBJECT (pad, "Finish dropping buffers until key frame");

  /* So this buffer is a keyframe we don't need this probe any more */
  return GST_PAD_PROBE_REMOVE;
}

void
kms_utils_drop_until_keyframe (GstPad * pad, gboolean all_headers)
{
  GST_OBJECT_LOCK (pad);
  if (is_dropping (pad)) {
    GST_DEBUG_OBJECT (pad, "Already dropping buffers until key frame");
    GST_OBJECT_UNLOCK (pad);
  } else {
    GST_DEBUG_OBJECT (pad, "Start dropping buffers until key frame");
    set_dropping (pad, TRUE);
    GST_OBJECT_UNLOCK (pad);
    gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        drop_until_keyframe_probe, GINT_TO_POINTER (all_headers), NULL);
    send_force_key_unit_event (pad, all_headers);
  }
}

static GstPadProbeReturn
discont_detection_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)
      || GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_GAP)) {
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
      GST_WARNING_OBJECT (pad, "Discont detected");
      kms_utils_drop_until_keyframe (pad, FALSE);

      return GST_PAD_PROBE_DROP;
    }
  }

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
gap_detection_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  if (GST_EVENT_TYPE (event) == GST_EVENT_GAP) {
    GST_WARNING_OBJECT (pad, "Gap detected");
    kms_utils_drop_until_keyframe (pad, FALSE);
  }

  return GST_PAD_PROBE_OK;
}

void
kms_utils_manage_gaps (GstPad * pad)
{
  // TODO: Think about adding it here automatically or not
  kms_utils_control_key_frames_request_duplicates (pad);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, discont_detection_probe,
      NULL, NULL);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      gap_detection_probe, NULL, NULL);
}

static gboolean
check_last_request_time (GstPad * pad)
{
  GstClockTime *last, now;
  GstClock *clock;
  GstElement *element = gst_pad_get_parent_element (pad);
  gboolean ret = FALSE;

  if (element == NULL) {
    GST_ERROR_OBJECT (pad, "Cannot get parent object to get clock");
    return TRUE;
  }

  clock = gst_element_get_clock (element);
  now = gst_clock_get_time (clock);
  g_object_unref (clock);
  g_object_unref (element);

  GST_OBJECT_LOCK (pad);

  last = g_object_get_data (G_OBJECT (pad), LAST_KEY_FRAME_REQUEST_TIME);

  if (last == NULL) {
    last = g_slice_new (GstClockTime);
    g_object_set_data_full (G_OBJECT (pad), LAST_KEY_FRAME_REQUEST_TIME, last,
        (GDestroyNotify) kms_utils_destroy_GstClockTime);

    *last = now;
    ret = TRUE;
  } else if (((*last) + DEFAULT_KEYFRAME_DISPERSION) < now) {
    ret = TRUE;
    *last = now;
  }

  GST_OBJECT_UNLOCK (pad);

  return ret;
}

static GstPadProbeReturn
control_duplicates (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  if (gst_video_event_is_force_key_unit (event)) {
    if (check_last_request_time (pad)) {
      GST_TRACE_OBJECT (pad, "Sending keyframe request");
      return GST_PAD_PROBE_OK;
    } else {
      GST_TRACE_OBJECT (pad, "Dropping keyframe request");
      return GST_PAD_PROBE_DROP;
    }
  }

  return GST_PAD_PROBE_OK;
}

void
kms_utils_control_key_frames_request_duplicates (GstPad * pad)
{
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, control_duplicates,
      NULL, NULL);
}

void
kms_element_for_each_src_pad (GstElement * element,
    KmsPadIterationAction action, gpointer data)
{
  GstIterator *it = gst_element_iterate_src_pads (element);
  gboolean done = FALSE;
  GstPad *pad;
  GValue item = G_VALUE_INIT;

  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:
        pad = g_value_get_object (&item);
        action (pad, data);
        g_value_reset (&item);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  gst_iterator_free (it);
}

static void init_debug (void) __attribute__ ((constructor));

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}

/* Type destroying */
#define KMS_UTILS_DESTROY(type) \
  void kms_utils_destroy_##type (type * data) { \
    g_slice_free (type, data);                  \
  }                                             \

KMS_UTILS_DESTROY (guint64)
KMS_UTILS_DESTROY (gsize)
KMS_UTILS_DESTROY (GstClockTime)
