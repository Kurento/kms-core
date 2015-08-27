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

#define GST_CAT_DEFAULT kmsutils
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmsutils"
#define LAST_KEY_FRAME_REQUEST_TIME "kmslastkeyframe"

#define DEFAULT_KEYFRAME_DISPERSION GST_SECOND  /* 1s */

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

gboolean
kms_utils_caps_are_raw (const GstCaps * caps)
{
  gboolean ret;
  GstCaps *raw_caps = gst_caps_from_string (KMS_AGNOSTIC_RAW_CAPS);

  ret = gst_caps_is_always_compatible (caps, raw_caps);

  gst_caps_unref (raw_caps);

  return ret;
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

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
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
    send_force_key_unit_event (pad, FALSE);
    return GST_PAD_PROBE_DROP;
  }

  return GST_PAD_PROBE_OK;
}

void
kms_utils_manage_gaps (GstPad * pad)
{
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
    KmsPadCallback action, gpointer data)
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

  g_value_unset (&item);
  gst_iterator_free (it);
}

typedef struct _PadBlockedData
{
  KmsPadCallback callback;
  gpointer userData;
  gboolean drop;
  gchar *eventId;
} PadBlockedData;

static gchar *
create_random_string (gsize size)
{
  const gchar charset[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const guint charset_size = sizeof (charset) - 1;

  gchar *s = g_malloc (size + 1);

  if (!s) {
    return NULL;
  }

  if (size) {
    gsize n;

    for (n = 0; n < size; n++) {
      s[n] = charset[g_random_int_range (0, charset_size)];
    }
  }

  s[size] = '\0';

  return s;
}

/*
 * This function sends a dummy event to force blocked probe to be called
 */
static void
send_dummy_event (GstPad * pad, const gchar * name)
{
  GstElement *parent = gst_pad_get_parent_element (pad);

  if (parent == NULL) {
    return;
  }

  if (GST_PAD_IS_SINK (pad)) {
    gst_pad_send_event (pad,
        gst_event_new_custom (GST_EVENT_TYPE_DOWNSTREAM |
            GST_EVENT_TYPE_SERIALIZED, gst_structure_new_empty (name)));
  } else {
    gst_pad_send_event (pad,
        gst_event_new_custom (GST_EVENT_TYPE_UPSTREAM |
            GST_EVENT_TYPE_SERIALIZED, gst_structure_new_empty (name)));
  }

  g_object_unref (parent);
}

static GstPadProbeReturn
pad_blocked_callback (GstPad * pad, GstPadProbeInfo * info, gpointer d)
{
  PadBlockedData *data = d;
  GstEvent *event;
  const GstStructure *st;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_QUERY_BOTH) {
    /* Queries must be answered */
    return GST_PAD_PROBE_PASS;
  }

  if (!(GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_BOTH)) {
    goto end;
  }

  if (~GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BLOCK) {
    return data->drop ? GST_PAD_PROBE_DROP : GST_PAD_PROBE_OK;
  }

  event = GST_PAD_PROBE_INFO_EVENT (info);

  if (!(GST_EVENT_TYPE (event) & GST_EVENT_CUSTOM_BOTH)) {
    goto end;
  }

  st = gst_event_get_structure (event);

  if (g_strcmp0 (data->eventId, gst_structure_get_name (st)) != 0) {
    goto end;
  }

  data->callback (pad, data->userData);

  return GST_PAD_PROBE_DROP;

end:
  return data->drop ? GST_PAD_PROBE_DROP : GST_PAD_PROBE_PASS;
}

void
kms_utils_execute_with_pad_blocked (GstPad * pad, gboolean drop,
    KmsPadCallback func, gpointer userData)
{
  gulong probe_id;
  PadBlockedData *data = g_slice_new (PadBlockedData);

  data->callback = func;
  data->userData = userData;
  data->drop = drop;
  data->eventId = create_random_string (10);

  probe_id = gst_pad_add_probe (pad,
      (GstPadProbeType) (GST_PAD_PROBE_TYPE_BLOCK),
      pad_blocked_callback, data, NULL);

  send_dummy_event (pad, data->eventId);

  gst_pad_remove_probe (pad, probe_id);

  g_free (data->eventId);
  g_slice_free (PadBlockedData, data);
}

/* REMB event begin */

#define KMS_REMB_EVENT_NAME "REMB"
#define REMB_HASH_CLEAR_INTERVAL 10 * GST_SECOND

GstEvent *
kms_utils_remb_event_upstream_new (guint bitrate, guint ssrc)
{
  GstEvent *event;

  event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM,
      gst_structure_new (KMS_REMB_EVENT_NAME,
          "bitrate", G_TYPE_UINT, bitrate, "ssrc", G_TYPE_UINT, ssrc, NULL));

  return event;
}

static gboolean
is_remb_event (GstEvent * event)
{
  const GstStructure *s;

  g_return_val_if_fail (event != NULL, FALSE);

  if (GST_EVENT_TYPE (event) != GST_EVENT_CUSTOM_UPSTREAM) {
    return FALSE;
  }

  s = gst_event_get_structure (event);
  g_return_val_if_fail (s != NULL, FALSE);

  return gst_structure_has_name (s, KMS_REMB_EVENT_NAME);
}

gboolean
kms_utils_remb_event_upstream_parse (GstEvent * event, guint * bitrate,
    guint * ssrc)
{
  const GstStructure *s;

  if (!is_remb_event (event)) {
    return FALSE;
  }

  s = gst_event_get_structure (event);
  g_return_val_if_fail (s != NULL, FALSE);

  if (!gst_structure_get_uint (s, "bitrate", bitrate)) {
    return FALSE;
  }
  if (!gst_structure_get_uint (s, "ssrc", ssrc)) {
    return FALSE;
  }

  return TRUE;
}

struct _RembEventManager
{
  GMutex mutex;
  guint remb_min;
  GHashTable *remb_hash;
  GstPad *pad;
  gulong probe_id;
  GstClockTime oldest_remb_value;

  /* Callback */
  RembBitrateUpdatedCallback callback;
  gpointer user_data;
  GDestroyNotify user_data_destroy;
};

typedef struct _RembHashValue
{
  guint bitrate;
  GstClockTime ts;
} RembHashValue;

static RembHashValue *
remb_hash_value_create (guint bitrate)
{
  RembHashValue *value = g_slice_new0 (RembHashValue);

  value->bitrate = bitrate;
  value->ts = kms_utils_get_time_nsecs ();

  return value;
}

static void
remb_hash_value_destroy (gpointer value)
{
  g_slice_free (RembHashValue, value);
}

static void
remb_event_manager_set_min (RembEventManager * manager, guint min)
{
  if (manager->remb_min != min) {
    manager->remb_min = min;

    if (manager->callback) {
      // TODO: Think about having a threshold to not notify in excess
      manager->callback (manager, manager->remb_min, manager->user_data);
    }
  }
}

static void
remb_event_manager_calc_min (RembEventManager * manager)
{
  guint remb_min = 0;
  GstClockTime time = kms_utils_get_time_nsecs ();
  GstClockTime oldest_time = GST_CLOCK_TIME_NONE;
  GHashTableIter iter;
  gpointer key, v;

  g_hash_table_iter_init (&iter, manager->remb_hash);
  while (g_hash_table_iter_next (&iter, &key, &v)) {
    guint br = ((RembHashValue *) v)->bitrate;
    GstClockTime ts = ((RembHashValue *) v)->ts;

    if (time - ts > REMB_HASH_CLEAR_INTERVAL) {
      GST_TRACE ("Remove entry %" G_GUINT32_FORMAT, GPOINTER_TO_UINT (key));
      g_hash_table_iter_remove (&iter);
      continue;
    }

    if (remb_min == 0) {
      remb_min = br;
    } else {
      remb_min = MIN (remb_min, br);
    }

    oldest_time = MIN (oldest_time, ts);
  }

  manager->oldest_remb_value = oldest_time;
  remb_event_manager_set_min (manager, remb_min);
}

static void
remb_event_manager_update_min (RembEventManager * manager, guint bitrate,
    guint ssrc)
{
  RembHashValue *last_value, *value;

  g_mutex_lock (&manager->mutex);
  last_value = g_hash_table_lookup (manager->remb_hash,
      GUINT_TO_POINTER (ssrc));

  if (last_value != NULL && bitrate == last_value->bitrate) {
    last_value->ts = kms_utils_get_time_nsecs ();
    goto end;
  }

  value = remb_hash_value_create (bitrate);
  g_hash_table_insert (manager->remb_hash, GUINT_TO_POINTER (ssrc), value);

  if (bitrate > manager->remb_min) {
    remb_event_manager_calc_min (manager);
  } else {
    remb_event_manager_set_min (manager, bitrate);
  }

end:

  GST_TRACE_OBJECT (manager->pad, "remb_min: %" G_GUINT32_FORMAT,
      manager->remb_min);

  g_mutex_unlock (&manager->mutex);
}

static GstPadProbeReturn
remb_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  RembEventManager *manager = user_data;
  GstEvent *event = gst_pad_probe_info_get_event (info);
  guint bitrate, ssrc;

  if (!kms_utils_remb_event_upstream_parse (event, &bitrate, &ssrc)) {
    return GST_PAD_PROBE_OK;
  }

  GST_TRACE_OBJECT (pad, "<%" G_GUINT32_FORMAT ", %" G_GUINT32_FORMAT ">", ssrc,
      bitrate);

  remb_event_manager_update_min (manager, bitrate, ssrc);

  return GST_PAD_PROBE_DROP;
}

RembEventManager *
kms_utils_remb_event_manager_create (GstPad * pad)
{
  RembEventManager *manager = g_slice_new0 (RembEventManager);

  g_mutex_init (&manager->mutex);
  manager->remb_hash =
      g_hash_table_new_full (NULL, NULL, NULL, remb_hash_value_destroy);
  manager->pad = g_object_ref (pad);
  manager->probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      remb_probe, manager, NULL);
  manager->oldest_remb_value = kms_utils_get_time_nsecs ();

  return manager;
}

void
kms_utils_remb_event_manager_destroy_user_data (RembEventManager * manager)
{
  if (manager->user_data && manager->user_data_destroy) {
    manager->user_data_destroy (manager->user_data);
  }
  manager->user_data = NULL;
  manager->user_data_destroy = NULL;
}

void
kms_utils_remb_event_manager_destroy (RembEventManager * manager)
{
  kms_utils_remb_event_manager_destroy_user_data (manager);

  gst_pad_remove_probe (manager->pad, manager->probe_id);
  g_object_unref (manager->pad);
  g_hash_table_destroy (manager->remb_hash);
  g_mutex_clear (&manager->mutex);
  g_slice_free (RembEventManager, manager);
}

void
kms_utils_remb_event_manager_pointer_destroy (gpointer manager)
{
  kms_utils_remb_event_manager_destroy ((RembEventManager *) manager);
}

guint
kms_utils_remb_event_manager_get_min (RembEventManager * manager)
{
  GstClockTime time = kms_utils_get_time_nsecs ();
  guint ret;

  g_mutex_lock (&manager->mutex);
  if (time - manager->oldest_remb_value > REMB_HASH_CLEAR_INTERVAL) {
    remb_event_manager_calc_min (manager);
  }

  ret = manager->remb_min;
  g_mutex_unlock (&manager->mutex);

  return ret;
}

void
kms_utils_remb_event_manager_set_callback (RembEventManager * manager,
    RembBitrateUpdatedCallback cb, gpointer data, GDestroyNotify destroy_notify)
{
  g_mutex_lock (&manager->mutex);
  kms_utils_remb_event_manager_destroy_user_data (manager);

  manager->user_data = data;
  manager->user_data_destroy = destroy_notify;
  manager->callback = cb;
  g_mutex_unlock (&manager->mutex);
}

/* REMB event end */

/* time begin */

GstClockTime
kms_utils_get_time_nsecs ()
{
  GstClockTime time;

  time = g_get_monotonic_time () * GST_USECOND;

  return time;
}

/* time end */

static void init_debug (void) __attribute__ ((constructor));

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}

/* Type destroying */
#define KMS_UTILS_DESTROY(type)                 \
  void kms_utils_destroy_##type (type * data) { \
    g_slice_free (type, data);                  \
  }

KMS_UTILS_DESTROY (guint64);
KMS_UTILS_DESTROY (gsize);
KMS_UTILS_DESTROY (GstClockTime);
KMS_UTILS_DESTROY (gfloat);
KMS_UTILS_DESTROY (guint);
