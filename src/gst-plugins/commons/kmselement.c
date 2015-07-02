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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "kms-core-enumtypes.h"
#include "kms-core-marshal.h"
#include "kmselement.h"
#include "kmsagnosticcaps.h"
#include "kmsutils.h"

#define PLUGIN_NAME "kmselement"
#define DEFAULT_ACCEPT_EOS TRUE
#define DEFAULT_BITRATE_ "default-bitrate"

GST_DEBUG_CATEGORY_STATIC (kms_element_debug_category);
#define GST_CAT_DEFAULT kms_element_debug_category

G_DEFINE_TYPE_WITH_CODE (KmsElement, kms_element,
    GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (kms_element_debug_category, PLUGIN_NAME,
        0, "debug category for element"));

#define SINK_PAD "sink_%s"
#define VIDEO_SRC_PAD "video_src_%u"
#define AUDIO_SRC_PAD "audio_src_%u"
#define DATA_SRC_PAD "data_src_%u"

#define KMS_ELEMENT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KMS_TYPE_ELEMENT, KmsElementPrivate))

#define KMS_ELEMENT_SYNC_LOCK(obj) (                      \
  g_mutex_lock (&(((KmsElement *)obj)->priv->sync_lock))  \
)

#define KMS_ELEMENT_SYNC_UNLOCK(obj) (                      \
  g_mutex_unlock (&(((KmsElement *)obj)->priv->sync_lock))  \
)

typedef struct _PendingSrcPad
{
  KmsElementPadType type;
  gchar *desc;
} PendingSrcPad;

struct _KmsElementPrivate
{
  guint audio_pad_count;
  guint video_pad_count;
  guint data_pad_count;

  gboolean accept_eos;

  GstElement *audio_agnosticbin;
  GstElement *video_agnosticbin;
  GstElement *data_tee;

  /* Audio and video capabilities */
  GstCaps *audio_caps;
  GstCaps *video_caps;

  GHashTable *pendingpads;

  gint target_bitrate;
};

/* Signals and args */
enum
{
  /* Actions */
  REQUEST_NEW_SRCPAD,
  RELEASE_REQUESTED_SRCPAD,
  LAST_SIGNAL
};

static guint element_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_ACCEPT_EOS,
  PROP_AUDIO_CAPS,
  PROP_VIDEO_CAPS,
  PROP_TARGET_BITRATE,
  PROP_LAST
};

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_src_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS)
    );

static GstStaticPadTemplate data_src_factory =
GST_STATIC_PAD_TEMPLATE (DATA_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_DATA_CAPS)
    );

static PendingSrcPad *
create_pendingpad (KmsElementPadType type, const gchar * desc)
{
  PendingSrcPad *data;

  data = g_slice_new0 (PendingSrcPad);
  data->type = type;
  data->desc = g_strdup (desc);

  return data;
}

static void
destroy_pendingpads (PendingSrcPad * data)
{
  if (data->desc != NULL) {
    g_free (data->desc);
  }

  g_slice_free (PendingSrcPad, data);
}

static void
kms_element_set_target_on_linked (GstPad * pad, GstPad * peer,
    GstElement * element)
{
  GstPad *target;

  target = gst_element_get_request_pad (element, "src_%u");

  if (GST_PAD_IS_FLUSHING (peer)) {
    gst_pad_send_event (peer, gst_event_new_flush_start ());
    gst_pad_send_event (peer, gst_event_new_flush_stop (FALSE));
  }

  GST_DEBUG_OBJECT (pad, "Setting target %" GST_PTR_FORMAT, target);

  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (pad), target)) {
    GST_ERROR_OBJECT (pad, "Can not set target pad");
  }

  g_object_unref (target);
}

static void
kms_element_remove_target_on_unlinked (GstPad * pad, GstPad * peer,
    GstElement * element)
{
  GstPad *target;

  target = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));

  if (target == NULL) {
    GST_DEBUG_OBJECT (pad, "No target pad");
    return;
  }

  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (pad), NULL)) {
    GST_ERROR_OBJECT (pad, "Can not remove target pad");
  }

  GST_DEBUG_OBJECT (element, "Removing requested pad %" GST_PTR_FORMAT, target);
  gst_element_release_request_pad (element, target);

  g_object_unref (target);
}

static gboolean
kms_element_get_data_by_type (KmsElement * self, KmsElementPadType type,
    const gchar ** templ_name, GstElement ** element)
{
  switch (type) {
    case KMS_ELEMENT_PAD_TYPE_DATA:
      *element = self->priv->data_tee;
      *templ_name = DATA_SRC_PAD;
      break;
    case KMS_ELEMENT_PAD_TYPE_AUDIO:
      *element = self->priv->audio_agnosticbin;
      *templ_name = AUDIO_SRC_PAD;
      break;
    case KMS_ELEMENT_PAD_TYPE_VIDEO:
      *element = self->priv->video_agnosticbin;
      *templ_name = VIDEO_SRC_PAD;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported pad type: %s",
          kms_element_pad_type_str (type));
      return FALSE;
  }

  return TRUE;
}

static void
kms_element_add_src_pad (KmsElement * self, GstElement * element,
    const gchar * pad_name, const gchar * templ_name)
{
  GstPad *srcpad;

  srcpad =
      gst_ghost_pad_new_no_target_from_template (pad_name,
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS
          (G_OBJECT_GET_CLASS (self)), templ_name));

  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED)
    gst_pad_set_active (srcpad, TRUE);

  g_signal_connect (srcpad, "linked",
      G_CALLBACK (kms_element_set_target_on_linked), element);

  g_signal_connect (srcpad, "unlinked",
      G_CALLBACK (kms_element_remove_target_on_unlinked), element);

  gst_element_add_pad (GST_ELEMENT (self), srcpad);
}

static void
kms_element_create_pending_pads (KmsElement * self, KmsElementPadType type)
{
  GHashTableIter iter;
  gpointer key, value;
  GstElement *element;
  const gchar *templ_name;
  GSList *keys = NULL, *l;

  KMS_ELEMENT_LOCK (self);

  if (!kms_element_get_data_by_type (self, type, &templ_name, &element)) {
    KMS_ELEMENT_UNLOCK (self);
    return;
  }

  g_hash_table_iter_init (&iter, self->priv->pendingpads);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    PendingSrcPad *pendingpad = value;

    /* TODO: Discriminate pads using their description */
    if (pendingpad->type != type) {
      continue;
    }

    keys = g_slist_prepend (keys, g_strdup (key));
    g_hash_table_iter_remove (&iter);
  }

  KMS_ELEMENT_UNLOCK (self);

  /* Create all pending pads */
  for (l = keys; l != NULL; l = l->next) {
    kms_element_add_src_pad (self, element, l->data, templ_name);
  }

  g_slist_free_full (keys, g_free);
}

GstElement *
kms_element_get_data_tee (KmsElement * self)
{
  GstElement *sink, *tee;

  GST_DEBUG_OBJECT (self, "Data tee requested");
  KMS_ELEMENT_LOCK (self);
  if (self->priv->data_tee != NULL) {
    KMS_ELEMENT_UNLOCK (self);
    return self->priv->data_tee;
  }

  tee = gst_element_factory_make ("tee", NULL);

  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (sink, "sync", FALSE, "async", FALSE, NULL);

  gst_bin_add_many (GST_BIN (self), tee, sink, NULL);
  gst_element_link (tee, sink);

  self->priv->data_tee = tee;
  KMS_ELEMENT_UNLOCK (self);

  gst_element_sync_state_with_parent (sink);
  gst_element_sync_state_with_parent (tee);

  kms_element_create_pending_pads (self, KMS_ELEMENT_PAD_TYPE_DATA);

  return tee;
}

GstElement *
kms_element_get_audio_agnosticbin (KmsElement * self)
{
  GST_DEBUG_OBJECT (self, "Audio agnostic requested");
  KMS_ELEMENT_LOCK (self);
  if (self->priv->audio_agnosticbin != NULL) {
    KMS_ELEMENT_UNLOCK (self);
    return self->priv->audio_agnosticbin;
  }

  self->priv->audio_agnosticbin =
      gst_element_factory_make ("agnosticbin", NULL);

  gst_bin_add (GST_BIN (self), self->priv->audio_agnosticbin);
  gst_element_sync_state_with_parent (self->priv->audio_agnosticbin);
  KMS_ELEMENT_UNLOCK (self);

  kms_element_create_pending_pads (self, KMS_ELEMENT_PAD_TYPE_AUDIO);

  return self->priv->audio_agnosticbin;
}

GstElement *
kms_element_get_video_agnosticbin (KmsElement * self)
{
  GST_DEBUG_OBJECT (self, "Video agnostic requested");
  KMS_ELEMENT_LOCK (self);
  if (self->priv->video_agnosticbin != NULL) {
    KMS_ELEMENT_UNLOCK (self);
    return self->priv->video_agnosticbin;
  }

  self->priv->video_agnosticbin =
      gst_element_factory_make ("agnosticbin", NULL);

  gst_bin_add (GST_BIN (self), self->priv->video_agnosticbin);
  gst_element_sync_state_with_parent (self->priv->video_agnosticbin);
  KMS_ELEMENT_UNLOCK (self);

  kms_element_create_pending_pads (self, KMS_ELEMENT_PAD_TYPE_VIDEO);

  return self->priv->video_agnosticbin;
}

static void
send_flush_on_unlink (GstPad * pad, GstPad * peer, gpointer user_data)
{
  if (GST_OBJECT_FLAG_IS_SET (pad, GST_PAD_FLAG_EOS)) {
    gst_pad_send_event (pad, gst_event_new_flush_start ());
    gst_pad_send_event (pad, gst_event_new_flush_stop (FALSE));
  }
}

static GstPadProbeReturn
accept_eos_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *event = gst_pad_probe_info_get_event (info);
  GstEventType type = GST_EVENT_TYPE (event);

  if (type == GST_EVENT_EOS || type == GST_EVENT_FLUSH_START
      || type == GST_EVENT_FLUSH_STOP) {
    KmsElement *self;
    gboolean accept;

    self = KMS_ELEMENT (data);
    KMS_ELEMENT_LOCK (self);
    accept = self->priv->accept_eos;
    KMS_ELEMENT_UNLOCK (self);

    if (!accept) {
      GST_DEBUG_OBJECT (pad, "Event %s dropped",
          gst_event_type_get_name (type));
    }

    return (accept) ? GST_PAD_PROBE_OK : GST_PAD_PROBE_DROP;
  }

  return GST_PAD_PROBE_OK;
}

static gboolean
kms_element_sink_query_default (KmsElement * self, GstPad * pad,
    GstQuery * query)
{
  /* Invoke default pad query function */
  return gst_pad_query_default (pad, GST_OBJECT (self), query);
}

static gboolean
kms_element_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  KmsElementClass *klass;
  KmsElement *element;

  element = KMS_ELEMENT (parent);
  klass = KMS_ELEMENT_GET_CLASS (element);

  return klass->sink_query (element, pad, query);
}

const gchar *
kms_element_pad_type_str (KmsElementPadType type)
{
  switch (type) {
    case KMS_ELEMENT_PAD_TYPE_VIDEO:
      return "video";
    case KMS_ELEMENT_PAD_TYPE_AUDIO:
      return "audio";
    case KMS_ELEMENT_PAD_TYPE_DATA:
      return "data";
    default:
      return "";
  }
}

static gchar *
get_pad_name (KmsElementPadType type, const gchar * description)
{
  if (description == NULL) {
    return g_strdup_printf (SINK_PAD, kms_element_pad_type_str (type));
  } else {
    return g_strdup_printf (SINK_PAD "_%s", kms_element_pad_type_str (type),
        description);
  }
}

GstPad *
kms_element_connect_sink_target_full (KmsElement * self, GstPad * target,
    KmsElementPadType type, const gchar * description)
{
  GstPad *pad;
  gchar *pad_name;
  GstPadTemplate *templ;

  templ = gst_static_pad_template_get (&sink_factory);

  pad_name = get_pad_name (type, description);

  pad = gst_ghost_pad_new_from_template (pad_name, target, templ);
  g_free (pad_name);
  g_object_unref (templ);

  if (type == KMS_ELEMENT_PAD_TYPE_VIDEO) {
    kms_utils_drop_until_keyframe (pad, TRUE);
    kms_utils_manage_gaps (pad);
  }

  gst_pad_set_query_function (pad, kms_element_pad_query);
  gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH,
      accept_eos_probe, self, NULL);
  g_signal_connect (G_OBJECT (pad), "unlinked",
      G_CALLBACK (send_flush_on_unlink), NULL);

  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, TRUE);
  }

  if (gst_element_add_pad (GST_ELEMENT (self), pad)) {
    return pad;
  }

  g_object_unref (pad);

  return NULL;
}

void
kms_element_remove_sink (KmsElement * self, GstPad * pad)
{
  g_return_if_fail (self);
  g_return_if_fail (pad);

  // TODO: Unlink correctly pad before removing it
  gst_ghost_pad_set_target (GST_GHOST_PAD (pad), NULL);
  gst_element_remove_pad (GST_ELEMENT (self), pad);
}

void
kms_element_remove_sink_by_type_full (KmsElement * self,
    KmsElementPadType type, const gchar * description)
{
  gchar *pad_name = get_pad_name (type, description);
  GstPad *pad = gst_element_get_static_pad (GST_ELEMENT (self), pad_name);

  if (pad == NULL) {
    GST_WARNING_OBJECT (self, "Cannot get pad %s", pad_name);
    goto end;
  }

  kms_element_remove_sink (self, pad);
  g_object_unref (pad);

end:
  g_free (pad_name);
}

static void
kms_element_release_pad (GstElement * element, GstPad * pad)
{
  GstElement *agnosticbin;
  GstPad *target;

  if (g_str_has_prefix (GST_OBJECT_NAME (pad), "audio_src")) {
    agnosticbin = KMS_ELEMENT (element)->priv->audio_agnosticbin;
  } else if (g_str_has_prefix (GST_OBJECT_NAME (pad), "video_src")) {
    agnosticbin = KMS_ELEMENT (element)->priv->video_agnosticbin;
  } else {
    return;
  }

  // TODO: Remove pad if is a sinkpad

  target = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));

  if (target != NULL) {
    if (agnosticbin != NULL) {
      gst_element_release_request_pad (agnosticbin, target);
    }
    g_object_unref (target);
  }

  gst_pad_push_event (pad, gst_event_new_flush_start ());

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, FALSE);
  }

  gst_pad_push_event (pad, gst_event_new_flush_stop (FALSE));

  gst_element_remove_pad (element, pad);
}

static void
kms_element_endpoint_set_caps (KmsElement * self, const GstCaps * caps,
    GstCaps ** out)
{
  GstCaps *old;

  GST_DEBUG_OBJECT (self, "setting caps to %" GST_PTR_FORMAT, caps);

  old = *out;

  if (caps != NULL) {
    *out = gst_caps_copy (caps);
  } else {
    *out = NULL;
  }

  if (old != NULL) {
    gst_caps_unref (old);
  }
}

static GstCaps *
kms_element_endpoint_get_caps (KmsElement * self, GstCaps * caps)
{
  GstCaps *c;

  if ((c = caps) != NULL) {
    gst_caps_ref (caps);
  }

  GST_DEBUG_OBJECT (self, "caps: %" GST_PTR_FORMAT, c);

  return c;
}

static void
kms_element_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsElement *self = KMS_ELEMENT (object);

  switch (property_id) {
    case PROP_ACCEPT_EOS:
      KMS_ELEMENT_LOCK (self);
      self->priv->accept_eos = g_value_get_boolean (value);
      KMS_ELEMENT_UNLOCK (self);
      break;
    case PROP_AUDIO_CAPS:
      kms_element_endpoint_set_caps (self, gst_value_get_caps (value),
          &self->priv->audio_caps);
      break;
    case PROP_VIDEO_CAPS:
      kms_element_endpoint_set_caps (self, gst_value_get_caps (value),
          &self->priv->video_caps);
      break;
    case PROP_TARGET_BITRATE:
      KMS_ELEMENT_LOCK (self);
      self->priv->target_bitrate = g_value_get_int (value);
      g_object_set (G_OBJECT (kms_element_get_video_agnosticbin (self)),
          DEFAULT_BITRATE_, self->priv->target_bitrate, NULL);
      KMS_ELEMENT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kms_element_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  KmsElement *self = KMS_ELEMENT (object);

  switch (property_id) {
    case PROP_ACCEPT_EOS:
      KMS_ELEMENT_LOCK (self);
      g_value_set_boolean (value, self->priv->accept_eos);
      KMS_ELEMENT_UNLOCK (self);
      break;
    case PROP_AUDIO_CAPS:
      g_value_take_boxed (value, kms_element_endpoint_get_caps (self,
              self->priv->audio_caps));
      break;
    case PROP_VIDEO_CAPS:
      g_value_take_boxed (value, kms_element_endpoint_get_caps (self,
              self->priv->video_caps));
      break;
    case PROP_TARGET_BITRATE:
      KMS_ELEMENT_LOCK (self);
      g_value_set_int (value, self->priv->target_bitrate);
      KMS_ELEMENT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kms_element_finalize (GObject * object)
{
  KmsElement *element = KMS_ELEMENT (object);

  GST_DEBUG_OBJECT (object, "finalize");

  /* free resources allocated by this object */
  g_hash_table_unref (element->priv->pendingpads);
  g_rec_mutex_clear (&element->mutex);

  if (element->priv->video_caps != NULL) {
    gst_caps_unref (element->priv->video_caps);
  }

  if (element->priv->audio_caps != NULL) {
    gst_caps_unref (element->priv->audio_caps);
  }

  /* chain up */
  G_OBJECT_CLASS (kms_element_parent_class)->finalize (object);
}

static gchar *
kms_element_request_new_srcpad_action (KmsElement * self,
    KmsElementPadType type, const gchar * desc)
{
  const gchar *templ_name;
  gchar *pad_name;
  GstElement *element;

  KMS_ELEMENT_LOCK (self);

  if (!kms_element_get_data_by_type (self, type, &templ_name, &element)) {
    KMS_ELEMENT_UNLOCK (self);
    return NULL;
  }

  switch (type) {
    case KMS_ELEMENT_PAD_TYPE_DATA:
      pad_name = g_strdup_printf (templ_name, self->priv->data_pad_count++);
      break;
    case KMS_ELEMENT_PAD_TYPE_AUDIO:
      pad_name = g_strdup_printf (templ_name, self->priv->audio_pad_count++);
      break;
    case KMS_ELEMENT_PAD_TYPE_VIDEO:
      pad_name = g_strdup_printf (templ_name, self->priv->video_pad_count++);
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported pad type %u", type);
      KMS_ELEMENT_UNLOCK (self);
      return NULL;
  }

  if (element == NULL) {
    PendingSrcPad *data;

    data = create_pendingpad (type, desc);
    g_hash_table_insert (self->priv->pendingpads, g_strdup (pad_name), data);
    KMS_ELEMENT_UNLOCK (self);

    return pad_name;
  }

  KMS_ELEMENT_UNLOCK (self);

  kms_element_add_src_pad (self, element, pad_name, templ_name);

  return pad_name;
}

static void
kms_element_remove_target_pad (KmsElement * self, GstPad * pad)
{
  GstElement *agnosticbin;
  GstPad *target;

  target = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
  if (target == NULL) {
    return;
  }

  agnosticbin = gst_pad_get_parent_element (target);
  if (agnosticbin == NULL) {
    GST_WARNING_OBJECT (self, "No agnosticbin owns %" GST_PTR_FORMAT, pad);
  } else {
    gst_element_release_request_pad (agnosticbin, target);
    g_object_unref (agnosticbin);
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD (pad), NULL);
  g_object_unref (target);
}

static gboolean
kms_element_release_requested_srcpad_action (KmsElement * self,
    const gchar * pad_name)
{
  GValue item = G_VALUE_INIT;
  gboolean done = FALSE;
  gboolean released;
  GstIterator *it;
  GstPad *pad;

  KMS_ELEMENT_LOCK (self);
  released = g_hash_table_remove (self->priv->pendingpads, pad_name);
  KMS_ELEMENT_UNLOCK (self);

  if (released) {
    /* Pad was not created yet */
    return TRUE;
  }

  /* Pad is not in the pending list so it may have been already created */
  it = gst_element_iterate_src_pads (GST_ELEMENT (self));

  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:{
        gchar *name;

        pad = g_value_get_object (&item);
        name = gst_pad_get_name (pad);
        if ((released = g_strcmp0 (name, pad_name) == 0)) {
          kms_element_remove_target_pad (self, pad);
          kms_element_release_pad (GST_ELEMENT (self), pad);
          done = TRUE;
        }
        g_value_reset (&item);
        g_free (name);
        break;
      }
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

  return released;
}

static void
kms_element_class_init (KmsElementClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = kms_element_set_property;
  gobject_class->get_property = kms_element_get_property;
  gobject_class->finalize = kms_element_finalize;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "KmsElement",
      "Base/Bin/KmsElement",
      "Base class for elements",
      "José Antonio Santos Cadenas <santoscadenas@kurento.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&data_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  g_object_class_install_property (gobject_class, PROP_ACCEPT_EOS,
      g_param_spec_boolean ("accept-eos",
          "Accept EOS",
          "Indicates if the element should accept EOS events.",
          DEFAULT_ACCEPT_EOS, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_AUDIO_CAPS,
      g_param_spec_boxed ("audio-caps", "Audio capabilities",
          "The allowed caps for audio", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_CAPS,
      g_param_spec_boxed ("video-caps", "Video capabilities",
          "The allowed caps for video", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_int ("output-bitrate", "output bitrate",
          "Configure the bitrate to media encoding",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  klass->sink_query = GST_DEBUG_FUNCPTR (kms_element_sink_query_default);

  /* set actions */
  element_signals[REQUEST_NEW_SRCPAD] =
      g_signal_new ("request-new-srcpad",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsElementClass, request_new_srcpad), NULL, NULL,
      __kms_core_marshal_STRING__ENUM_STRING,
      G_TYPE_STRING, 2, KMS_TYPE_ELEMENT_PAD_TYPE, G_TYPE_STRING);

  element_signals[RELEASE_REQUESTED_SRCPAD] =
      g_signal_new ("release-requested-srcpad",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsElementClass, release_requested_srcpad), NULL, NULL,
      __kms_core_marshal_BOOLEAN__STRING, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  klass->request_new_srcpad =
      GST_DEBUG_FUNCPTR (kms_element_request_new_srcpad_action);
  klass->release_requested_srcpad =
      GST_DEBUG_FUNCPTR (kms_element_release_requested_srcpad_action);

  g_type_class_add_private (klass, sizeof (KmsElementPrivate));
}

static void
kms_element_init (KmsElement * element)
{
  g_rec_mutex_init (&element->mutex);

  element->priv = KMS_ELEMENT_GET_PRIVATE (element);

  element->priv->accept_eos = DEFAULT_ACCEPT_EOS;
  element->priv->data_pad_count = 0;
  element->priv->audio_pad_count = 0;
  element->priv->video_pad_count = 0;
  element->priv->audio_agnosticbin = NULL;
  element->priv->video_agnosticbin = NULL;

  element->priv->pendingpads = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) destroy_pendingpads);
}

KmsElementPadType
kms_element_get_pad_type (KmsElement * self, GstPad * pad)
{
  KmsElementPadType type;
  GstPadTemplate *templ;

  g_return_val_if_fail (self != NULL, KMS_ELEMENT_PAD_TYPE_DATA);

  templ = gst_pad_get_pad_template (pad);

  if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), AUDIO_SRC_PAD) ||
      g_strstr_len (GST_OBJECT_NAME (pad), -1, "audio")) {
    type = KMS_ELEMENT_PAD_TYPE_AUDIO;
  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), VIDEO_SRC_PAD) ||
      g_strstr_len (GST_OBJECT_NAME (pad), -1, "video")) {
    type = KMS_ELEMENT_PAD_TYPE_VIDEO;
  } else {
    type = KMS_ELEMENT_PAD_TYPE_DATA;
  }

  gst_object_unref (templ);

  return type;
}
