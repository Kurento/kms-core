/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "kmsutils.h"
#include "constants.h"
#include "kmsagnosticcaps.h"
#include <gst/video/video-event.h>
#include <uuid/uuid.h>
#include <string.h>

#define GST_CAT_DEFAULT kmsutils
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmsutils"

#define LAST_KEY_FRAME_REQUEST_TIME "last-key-frame-request-time"
G_DEFINE_QUARK (LAST_KEY_FRAME_REQUEST_TIME, last_key_frame_request_time);

#define KMS_KEY_ID "kms-key-id"
G_DEFINE_QUARK (KMS_KEY_ID, kms_key_id);

#define DEFAULT_KEYFRAME_DISPERSION GST_SECOND  /* 1s */

#define UUID_STR_SIZE 37        /* 36-byte string (plus tailing '\0') */
#define BEGIN_CERTIFICATE "-----BEGIN CERTIFICATE-----"
#define END_CERTIFICATE "-----END CERTIFICATE-----"

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
static GstStaticCaps static_rtp_caps = GST_STATIC_CAPS (KMS_AGNOSTIC_RTP_CAPS);
static GstStaticCaps static_raw_caps =
    GST_STATIC_CAPS
    ("video/x-raw; video/x-raw(ANY); audio/x-raw; audio/x-raw(ANY);");

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
  GstCaps *raw_caps = gst_static_caps_get (&static_raw_caps);

  ret = gst_caps_is_always_compatible (caps, raw_caps);

  gst_caps_unref (raw_caps);

  return ret;
}

gboolean
kms_utils_caps_are_rtp (const GstCaps * caps)
{
  gboolean ret;
  GstCaps *raw_caps = gst_static_caps_get (&static_rtp_caps);

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
  GstElement *rate = NULL;

  if (kms_utils_caps_are_video (caps)) {
    rate = gst_element_factory_make ("videorate", NULL);
    g_object_set (G_OBJECT (rate), "average-period", GST_MSECOND * 200,
        "skip-to-first", TRUE, "drop-only", TRUE, NULL);
  }

  return rate;
}

const gchar *
kms_utils_get_caps_codec_name_from_sdp (const gchar * codec_name)
{
  if (g_ascii_strcasecmp (OPUS_ENCONDING_NAME, codec_name) == 0) {
    return "OPUS";
  }
  if (g_ascii_strcasecmp (VP8_ENCONDING_NAME, codec_name) == 0) {
    return "VP8";
  }
  if (g_ascii_strcasecmp (SPEEX_ENCONDING_NAME, codec_name) == 0) {
    return "SPEEX";
  }

  return codec_name;
}

/* key frame management */

#define DROPPING_UNTIL_KEY_FRAME "dropping-until-key-frame"
G_DEFINE_QUARK (DROPPING_UNTIL_KEY_FRAME, dropping_until_key_frame);

/* Call this function holding the lock */
static inline gboolean
is_dropping (GstPad * pad)
{
  return GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (pad),
          dropping_until_key_frame_quark ()));
}

/* Call this function holding the lock */
static inline void
set_dropping (GstPad * pad, gboolean dropping)
{
  g_object_set_qdata (G_OBJECT (pad), dropping_until_key_frame_quark (),
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

  last =
      g_object_get_qdata (G_OBJECT (pad), last_key_frame_request_time_quark ());

  if (last == NULL) {
    last = g_slice_new (GstClockTime);
    g_object_set_qdata_full (G_OBJECT (pad),
        last_key_frame_request_time_quark (), last,
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

static gboolean
kms_element_iterate_pads (GstIterator * it, KmsPadCallback action,
    gpointer data)
{
  gboolean done = FALSE;
  GstPad *pad;
  GValue item = G_VALUE_INIT;
  gboolean success = TRUE;

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
        success = FALSE;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  g_value_unset (&item);

  return success;
}

void
kms_element_for_each_src_pad (GstElement * element,
    KmsPadCallback action, gpointer data)
{
  GstIterator *it = gst_element_iterate_src_pads (element);

  kms_element_iterate_pads (it, action, data);
  gst_iterator_free (it);
}

gboolean
kms_element_for_each_sink_pad (GstElement * element,
    KmsPadCallback action, gpointer data)
{
  GstIterator *it = gst_element_iterate_sink_pads (element);
  gboolean ret;

  ret = kms_element_iterate_pads (it, action, data);
  gst_iterator_free (it);

  return ret;
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
#define DEFAULT_CLEAR_INTERVAL 10 * GST_SECOND

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
  GstClockTime oldest_remb_time;
  GstClockTime clear_interval;

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
remb_event_manager_calc_min (RembEventManager * manager, guint default_min)
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

    if (time - ts > manager->clear_interval) {
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

  if (remb_min == 0 && default_min > 0) {
    GST_DEBUG_OBJECT (manager->pad, "Setting default value: %" G_GUINT32_FORMAT,
        default_min);
    remb_min = default_min;
  }

  manager->oldest_remb_time = oldest_time;
  remb_event_manager_set_min (manager, remb_min);
}

static void
remb_event_manager_update_min (RembEventManager * manager, guint bitrate,
    guint ssrc)
{
  RembHashValue *last_value;
  GstClockTime time = kms_utils_get_time_nsecs ();
  gboolean new_br = TRUE;

  g_mutex_lock (&manager->mutex);
  last_value = g_hash_table_lookup (manager->remb_hash,
      GUINT_TO_POINTER (ssrc));

  if (last_value != NULL) {
    new_br = bitrate != last_value->bitrate;
    last_value->bitrate = bitrate;
    last_value->ts = kms_utils_get_time_nsecs ();
  } else {
    RembHashValue *value;

    value = remb_hash_value_create (bitrate);
    g_hash_table_insert (manager->remb_hash, GUINT_TO_POINTER (ssrc), value);
  }

  if (bitrate < manager->remb_min) {
    remb_event_manager_set_min (manager, bitrate);
  } else {
    gboolean calc_min;

    calc_min = new_br && (bitrate > manager->remb_min);
    calc_min = calc_min
        || (time - manager->oldest_remb_time > manager->clear_interval);
    if (calc_min) {
      remb_event_manager_calc_min (manager, bitrate);
    }
  }

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
  manager->oldest_remb_time = kms_utils_get_time_nsecs ();
  manager->clear_interval = DEFAULT_CLEAR_INTERVAL;

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
  if (time - manager->oldest_remb_time > manager->clear_interval) {
    remb_event_manager_calc_min (manager, 0);
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

void
kms_utils_remb_event_manager_set_clear_interval (RembEventManager * manager,
    GstClockTime interval)
{
  manager->clear_interval = interval;
}

GstClockTime
kms_utils_remb_event_manager_get_clear_interval (RembEventManager * manager)
{
  return manager->clear_interval;
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

/* RTP connection begin */
gchar *
kms_utils_create_connection_name_from_media_config (SdpMediaConfig * mconf)
{
  SdpMediaGroup *group = kms_sdp_media_config_get_group (mconf);
  gchar *conn_name;

  if (group != NULL) {
    gint gid = kms_sdp_media_group_get_id (group);

    conn_name =
        g_strdup_printf ("%s%" G_GINT32_FORMAT, BUNDLE_STREAM_NAME, gid);
  } else {
    gint mid = kms_sdp_media_config_get_id (mconf);

    conn_name = g_strdup_printf ("%" G_GINT32_FORMAT, mid);
  }

  return conn_name;
}

/* RTP connection end */

gboolean
kms_utils_contains_proto (const gchar * search_term, const gchar * proto)
{
  gchar *pattern;
  GRegex *regex;
  gboolean ret;

  pattern = g_strdup_printf ("(%s|.+/%s|%s/.+|.+/%s/.+)", proto, proto, proto,
      proto);
  regex = g_regex_new (pattern, 0, 0, NULL);
  ret = g_regex_match (regex, search_term, G_REGEX_MATCH_ANCHORED, NULL);
  g_regex_unref (regex);
  g_free (pattern);

  return ret;
}

const GstStructure *
kms_utils_get_structure_by_name (const GstStructure * str, const gchar * name)
{
  const GValue *value;

  value = gst_structure_get_value (str, name);

  if (value == NULL) {
    return NULL;
  }

  if (!GST_VALUE_HOLDS_STRUCTURE (value)) {
    gchar *str_val;

    str_val = g_strdup_value_contents (value);
    GST_WARNING ("Unexpected field type (%s) = %s", name, str_val);
    g_free (str_val);

    return NULL;
  }

  return gst_value_get_structure (value);
}

gchar *
kms_utils_generate_uuid ()
{
  gchar *uuid_str;
  uuid_t uuid;

  uuid_str = (gchar *) g_malloc0 (UUID_STR_SIZE);
  uuid_generate (uuid);
  uuid_unparse (uuid, uuid_str);

  return uuid_str;
}

void
kms_utils_set_uuid (GObject * obj)
{
  gchar *uuid_str;

  uuid_str = kms_utils_generate_uuid ();

  g_object_set_qdata_full (obj, kms_key_id_quark (), uuid_str, g_free);
}

const gchar *
kms_utils_get_uuid (GObject * obj)
{
  return (const gchar *) g_object_get_qdata (obj, kms_key_id_quark ());
}

const char *
kms_utils_media_type_to_str (KmsMediaType type)
{
  switch (type) {
    case KMS_MEDIA_TYPE_VIDEO:
      return "video";
    case KMS_MEDIA_TYPE_AUDIO:
      return "audio";
    case KMS_MEDIA_TYPE_DATA:
      return "data";
    default:
      return "<unsupported>";
  }
}

gchar *
kms_utils_generate_fingerprint_from_pem (const gchar * pem)
{
  guint i;
  gchar *line;
  guchar *der, *tmp;
  gchar **lines;
  gint state = 0;
  guint save = 0;
  gsize der_length = 0;
  GChecksum *checksum;
  guint8 *digest;
  gsize digest_length;
  GString *fingerprint;
  gchar *ret;

  if (pem == NULL) {
    GST_ERROR ("Pem certificate is null");
    return NULL;
  }
  der = tmp = g_new0 (guchar, (strlen (pem) / 4) * 3 + 3);
  lines = g_strsplit (pem, "\n", 0);

  for (i = 0, line = lines[i]; line; line = lines[++i]) {
    if (line[0] && g_str_has_prefix (line, BEGIN_CERTIFICATE)) {
      i++;
      break;
    }
  }

  for (line = lines[i]; line; line = lines[++i]) {
    if (line[0] && g_str_has_prefix (line, END_CERTIFICATE)) {
      break;
    }
    tmp += g_base64_decode_step (line, strlen (line), tmp, &state, &save);
  }
  der_length = tmp - der;
  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  digest_length = g_checksum_type_get_length (G_CHECKSUM_SHA256);
  digest = g_new (guint8, digest_length);
  g_checksum_update (checksum, der, der_length);
  g_checksum_get_digest (checksum, digest, &digest_length);
  fingerprint = g_string_new (NULL);
  for (i = 0; i < digest_length; i++) {
    if (i)
      g_string_append (fingerprint, ":");
    g_string_append_printf (fingerprint, "%02X", digest[i]);
  }
  ret = g_string_free (fingerprint, FALSE);

  g_free (digest);
  g_checksum_free (checksum);
  g_free (der);
  g_strfreev (lines);

  return ret;
}

typedef struct _KmsEventData KmsEventData;
struct _KmsEventData
{
  GstPadEventFunction user_func;
  gpointer user_data;
  GDestroyNotify user_notify;
  KmsEventData *next;
};

static void
kms_event_data_destroy (gpointer user_data)
{
  KmsEventData *data = user_data;

  if (data->next != NULL) {
    kms_event_data_destroy (data->next);
  }

  if (data->user_notify != NULL) {
    data->user_notify (data->user_data);
  }

  g_slice_free (KmsEventData, data);
}

static gboolean
kms_event_function (GstPad * pad, GstObject * parent, GstEvent * event)
{
  KmsEventData *data, *first = pad->eventdata;
  gboolean ret = TRUE;

  for (data = first; data != NULL && ret; data = data->next) {
    if (data->user_func != NULL) {
      /* Set data expected by the callback */
      pad->eventdata = data->user_data;
      ret = data->user_func (pad, parent, event);
    }
  }

  /* Restore pad event data */
  pad->eventdata = first;

  return ret;
}

void
kms_utils_set_pad_event_function_full (GstPad * pad, GstPadEventFunction event,
    gpointer user_data, GDestroyNotify notify, gboolean chain_callbacks)
{
  GstPadEventFunction prev_func;
  KmsEventData *data;

  /* Create new data */
  data = g_slice_new0 (KmsEventData);
  data->user_func = event;
  data->user_data = user_data;
  data->user_notify = notify;

  if (!chain_callbacks) {
    goto set_func;
  }

  prev_func = GST_PAD_EVENTFUNC (pad);

  if (prev_func != kms_event_function) {
    /* Keep first data to chain to it */
    KmsEventData *first;

    first = g_slice_new0 (KmsEventData);
    first->user_func = GST_PAD_EVENTFUNC (pad);
    first->user_data = pad->eventdata;
    first->user_notify = pad->eventnotify;
    data->next = first;
  } else {
    /* Point to previous data to be chained  */
    data->next = pad->eventdata;
  }

  /* Do not destroy previous data when set_event is called */
  pad->eventnotify = NULL;
  pad->eventdata = NULL;

set_func:

  gst_pad_set_event_function_full (pad, kms_event_function, data,
      kms_event_data_destroy);
}

typedef struct _KmsQueryData KmsQueryData;
struct _KmsQueryData
{
  GstPadQueryFunction user_func;
  gpointer user_data;
  GDestroyNotify user_notify;
  KmsQueryData *next;
};

static void
kms_query_data_destroy (gpointer user_data)
{
  KmsQueryData *data = user_data;

  if (data->next != NULL) {
    kms_query_data_destroy (data->next);
  }

  if (data->user_notify != NULL) {
    data->user_notify (data->user_data);
  }

  g_slice_free (KmsQueryData, data);
}

static gboolean
kms_query_function (GstPad * pad, GstObject * parent, GstQuery * query)
{
  KmsQueryData *data, *first = pad->querydata;
  gboolean ret = FALSE;

  for (data = first; data != NULL && !ret; data = data->next) {
    if (data->user_func != NULL) {
      /* Set data expected by the callback */
      pad->querydata = data->user_data;
      ret = data->user_func (pad, parent, query);
    }
  }

  /* Restore pad query data */
  pad->querydata = first;

  return ret;
}

void
kms_utils_set_pad_query_function_full (GstPad * pad,
    GstPadQueryFunction query_func, gpointer user_data, GDestroyNotify notify,
    gboolean chain_callbacks)
{
  GstPadQueryFunction prev_func;
  KmsQueryData *data;

  /* Create new data */
  data = g_slice_new0 (KmsQueryData);
  data->user_func = query_func;
  data->user_data = user_data;
  data->user_notify = notify;

  if (!chain_callbacks) {
    goto set_func;
  }

  prev_func = GST_PAD_QUERYFUNC (pad);

  if (prev_func != kms_query_function) {
    /* Keep first data to chain to it */
    KmsQueryData *first;

    first = g_slice_new0 (KmsQueryData);
    first->user_func = GST_PAD_QUERYFUNC (pad);
    first->user_data = pad->querydata;
    first->user_notify = pad->querynotify;
    data->next = first;
  } else {
    /* Point to previous data to be chained  */
    data->next = pad->querydata;
  }

  /* Do not destroy previous data when set_query is called */
  pad->querynotify = NULL;
  pad->querydata = NULL;

set_func:

  gst_pad_set_query_function_full (pad, kms_query_function, data,
      kms_query_data_destroy);
}

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
