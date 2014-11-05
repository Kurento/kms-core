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

#include "kmselement.h"
#include "kmsagnosticcaps.h"
#include "kmsutils.h"

#define PLUGIN_NAME "kmselement"
#define DEFAULT_ACCEPT_EOS TRUE

GST_DEBUG_CATEGORY_STATIC (kms_element_debug_category);
#define GST_CAT_DEFAULT kms_element_debug_category

G_DEFINE_TYPE_WITH_CODE (KmsElement, kms_element,
    GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (kms_element_debug_category, PLUGIN_NAME,
        0, "debug category for element"));

#define AUDIO_AGNOSTICBIN "audio_agnosticbin"
#define VIDEO_AGNOSTICBIN "video_agnosticbin"

#define AUDIO_SINK_PAD "audio_sink"
#define VIDEO_SINK_PAD "video_sink"

#define KMS_ELEMENT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KMS_TYPE_ELEMENT, KmsElementPrivate))

#define KMS_ELEMENT_SYNC_LOCK(obj) (                      \
  g_mutex_lock (&(((KmsElement *)obj)->priv->sync_lock))  \
)

#define KMS_ELEMENT_SYNC_UNLOCK(obj) (                      \
  g_mutex_unlock (&(((KmsElement *)obj)->priv->sync_lock))  \
)

struct _KmsElementPrivate
{
  guint audio_pad_count;
  guint video_pad_count;

  gboolean accept_eos;

  GstElement *audio_valve;
  GstElement *video_valve;

  GstElement *audio_agnosticbin;
  GstElement *video_agnosticbin;

  /* Audio and video capabilities */
  GstCaps *audio_caps;
  GstCaps *video_caps;

  /* Synchronization */
  GMutex sync_lock;
  GstClockTime base_time;
  GstClockTime base_clock;
};

/* Signals and args */
enum
{
  AGNOSTICBIN_ADDED,
  LAST_SIGNAL
};

static guint element_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_ACCEPT_EOS,
  PROP_AUDIO_CAPS,
  PROP_VIDEO_CAPS
};

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate audio_sink_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_sink_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS)
    );

static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE ("audio_src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_src_factory =
GST_STATIC_PAD_TEMPLATE ("video_src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS)
    );

static GstPadProbeReturn
synchronize_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  KmsElement *self = data;
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  buffer = gst_buffer_make_writable (buffer);
  info->data = buffer;

  KMS_ELEMENT_SYNC_LOCK (self);
  if (!GST_CLOCK_TIME_IS_VALID (self->priv->base_time)
      && GST_BUFFER_PTS_IS_VALID (buffer)) {
    self->priv->base_time = buffer->pts;
  }

  if (!GST_CLOCK_TIME_IS_VALID (self->priv->base_clock)) {
    GstObject *parent = GST_OBJECT (self);
    GstClock *clock;

    while (parent && parent->parent) {
      parent = parent->parent;
    }

    if (parent) {
      clock = gst_element_get_clock (GST_ELEMENT (parent));

      if (clock) {
        self->priv->base_clock =
            gst_clock_get_time (clock) -
            gst_element_get_base_time (GST_ELEMENT (parent));
        g_object_unref (clock);
      }
    }
  }

  if (GST_CLOCK_TIME_IS_VALID (self->priv->base_time)
      && GST_CLOCK_TIME_IS_VALID (self->priv->base_clock)) {
    if (GST_BUFFER_PTS_IS_VALID (buffer)) {
      if (self->priv->base_time > buffer->pts) {
        GST_WARNING_OBJECT (self,
            "Received a buffer with a pts lower than base");
        buffer->pts = self->priv->base_clock;
      } else {
        buffer->pts =
            (buffer->pts - self->priv->base_time) + self->priv->base_clock;
      }
    }
  } else {
    buffer->pts = GST_CLOCK_TIME_NONE;
  }
  KMS_ELEMENT_SYNC_UNLOCK (self);

  buffer->dts = buffer->pts;

  return GST_PAD_PROBE_OK;
}

GstElement *
kms_element_get_audio_agnosticbin (KmsElement * self)
{
  GstPad *sink;

  GST_DEBUG ("Audio agnostic requested");
  KMS_ELEMENT_LOCK (self);
  if (self->priv->audio_agnosticbin != NULL) {
    KMS_ELEMENT_UNLOCK (self);
    return self->priv->audio_agnosticbin;
  }

  self->priv->audio_agnosticbin =
      gst_element_factory_make ("agnosticbin", AUDIO_AGNOSTICBIN);

  sink = gst_element_get_static_pad (self->priv->audio_agnosticbin, "sink");
  gst_pad_add_probe (sink, GST_PAD_PROBE_TYPE_BUFFER, synchronize_probe, self,
      NULL);
  g_object_unref (sink);

  gst_bin_add (GST_BIN (self), self->priv->audio_agnosticbin);
  gst_element_sync_state_with_parent (self->priv->audio_agnosticbin);
  KMS_ELEMENT_UNLOCK (self);

  g_signal_emit (self, element_signals[AGNOSTICBIN_ADDED], 0);

  return self->priv->audio_agnosticbin;
}

GstElement *
kms_element_get_video_agnosticbin (KmsElement * self)
{
  GstPad *sink;

  GST_DEBUG ("Video agnostic requested");
  KMS_ELEMENT_LOCK (self);
  if (self->priv->video_agnosticbin != NULL) {
    KMS_ELEMENT_UNLOCK (self);
    return self->priv->video_agnosticbin;
  }

  self->priv->video_agnosticbin =
      gst_element_factory_make ("agnosticbin", VIDEO_AGNOSTICBIN);

  sink = gst_element_get_static_pad (self->priv->video_agnosticbin, "sink");
  gst_pad_add_probe (sink, GST_PAD_PROBE_TYPE_BUFFER, synchronize_probe, self,
      NULL);
  g_object_unref (sink);

  gst_bin_add (GST_BIN (self), self->priv->video_agnosticbin);
  gst_element_sync_state_with_parent (self->priv->video_agnosticbin);
  KMS_ELEMENT_UNLOCK (self);

  g_signal_emit (self, element_signals[AGNOSTICBIN_ADDED], 0);

  return self->priv->video_agnosticbin;
}

GstElement *
kms_element_get_audio_valve (KmsElement * self)
{
  return self->priv->audio_valve;
}

GstElement *
kms_element_get_video_valve (KmsElement * self)
{
  return self->priv->video_valve;
}

static GstPad *
kms_element_generate_sink_pad (KmsElement * element, const gchar * name,
    GstElement ** target, GstPadTemplate * templ)
{
  GstElement *valve = *target;
  GstPad *sink, *ret_pad;

  if (valve != NULL)
    return NULL;

  valve = gst_element_factory_make ("valve", name);
  *target = valve;
  g_object_set (valve, "drop", TRUE, NULL);
  gst_bin_add (GST_BIN (element), valve);
  gst_element_sync_state_with_parent (valve);

  if (g_str_has_prefix (name, "video")) {
    GstPad *src = gst_element_get_static_pad (valve, "src");

    kms_utils_drop_until_keyframe (src, TRUE);
    kms_utils_manage_gaps (src);
    g_object_unref (src);
  }

  if (target == &element->priv->audio_valve) {
    KMS_ELEMENT_GET_CLASS (element)->audio_valve_added (element, valve);
  } else {
    KMS_ELEMENT_GET_CLASS (element)->video_valve_added (element, valve);
  }

  sink = gst_element_get_static_pad (valve, "sink");
  ret_pad = gst_ghost_pad_new_from_template (name, sink, templ);
  g_object_unref (sink);

  return ret_pad;
}

static GstPad *
kms_element_generate_src_pad (KmsElement * element, const gchar * name,
    GstElement * agnosticbin, GstPadTemplate * templ)
{
  GstPad *agnostic_pad;
  GstPad *ret_pad;

  if (agnosticbin == NULL)
    return NULL;

  agnostic_pad = gst_element_get_request_pad (agnosticbin, "src_%u");
  ret_pad = gst_ghost_pad_new_from_template (name, agnostic_pad, templ);
  g_object_unref (agnostic_pad);

  return ret_pad;
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

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    KmsElement *self;
    gboolean accept;

    self = KMS_ELEMENT (data);
    KMS_ELEMENT_LOCK (self);
    accept = self->priv->accept_eos;
    KMS_ELEMENT_UNLOCK (self);

    if (!accept)
      GST_DEBUG_OBJECT (pad, "Eos dropped");

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

static GstPad *
kms_element_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *ret_pad = NULL;
  gchar *pad_name;
  gboolean added;

  if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), "audio_src_%u")) {
    KMS_ELEMENT_LOCK (element);
    pad_name = g_strdup_printf ("audio_src_%d",
        KMS_ELEMENT (element)->priv->audio_pad_count++);

    ret_pad = kms_element_generate_src_pad (KMS_ELEMENT (element), pad_name,
        KMS_ELEMENT (element)->priv->audio_agnosticbin, templ);

    if (ret_pad == NULL)
      KMS_ELEMENT (element)->priv->audio_pad_count--;

    KMS_ELEMENT_UNLOCK (element);

    g_free (pad_name);

  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), "video_src_%u")) {
    KMS_ELEMENT_LOCK (element);
    pad_name = g_strdup_printf ("video_src_%d",
        KMS_ELEMENT (element)->priv->video_pad_count++);

    ret_pad = kms_element_generate_src_pad (KMS_ELEMENT (element), pad_name,
        KMS_ELEMENT (element)->priv->video_agnosticbin, templ);

    if (ret_pad == NULL)
      KMS_ELEMENT (element)->priv->video_pad_count--;

    KMS_ELEMENT_UNLOCK (element);

    g_free (pad_name);

  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), AUDIO_SINK_PAD)) {
    KMS_ELEMENT_LOCK (element);
    ret_pad =
        kms_element_generate_sink_pad (KMS_ELEMENT (element), AUDIO_SINK_PAD,
        &KMS_ELEMENT (element)->priv->audio_valve, templ);

    gst_pad_set_query_function (ret_pad, kms_element_pad_query);
    gst_pad_add_probe (ret_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        accept_eos_probe, element, NULL);
    KMS_ELEMENT_UNLOCK (element);

    g_signal_connect (G_OBJECT (ret_pad), "unlinked",
        G_CALLBACK (send_flush_on_unlink), NULL);
  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), VIDEO_SINK_PAD)) {
    KMS_ELEMENT_LOCK (element);
    ret_pad =
        kms_element_generate_sink_pad (KMS_ELEMENT (element), VIDEO_SINK_PAD,
        &KMS_ELEMENT (element)->priv->video_valve, templ);

    gst_pad_set_query_function (ret_pad, kms_element_pad_query);
    gst_pad_add_probe (ret_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        accept_eos_probe, element, NULL);
    KMS_ELEMENT_UNLOCK (element);

    g_signal_connect (G_OBJECT (ret_pad), "unlinked",
        G_CALLBACK (send_flush_on_unlink), NULL);
  }

  if (ret_pad == NULL) {
    GST_WARNING ("No pad created");
    return NULL;
  }

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (element) >= GST_STATE_PAUSED)
    gst_pad_set_active (ret_pad, TRUE);

  added = gst_element_add_pad (element, ret_pad);

  if (added)
    return ret_pad;

  if (gst_pad_get_direction (ret_pad) == GST_PAD_SRC) {
    GstPad *target = gst_ghost_pad_get_target (GST_GHOST_PAD (ret_pad));

    if (target != NULL) {
      GstElement *agnostic = gst_pad_get_parent_element (target);

      gst_element_release_request_pad (agnostic, target);
      g_object_unref (target);
      g_object_unref (agnostic);
    }
  }

  g_object_unref (ret_pad);
  return NULL;
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

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED)
    gst_pad_set_active (pad, FALSE);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kms_element_finalize (GObject * object)
{
  KmsElement *element = KMS_ELEMENT (object);

  /* free resources allocated by this object */
  g_rec_mutex_clear (&element->mutex);

  g_mutex_clear (&element->priv->sync_lock);

  /* chain up */
  G_OBJECT_CLASS (kms_element_parent_class)->finalize (object);
}

static void
kms_element_audio_valve_added_default (KmsElement * self, GstElement * valve)
{
  GST_WARNING
      ("Element class %" GST_PTR_FORMAT
      " does not implement method \"audio_valve_added\"", self);
}

static void
kms_element_video_valve_added_default (KmsElement * self, GstElement * valve)
{
  GST_WARNING
      ("Element class %" GST_PTR_FORMAT
      " does not implement method \"video_valve_added\"", self);
}

static void
kms_element_audio_valve_removed_default (KmsElement * self, GstElement * valve)
{
  GST_WARNING
      ("Element class %" GST_PTR_FORMAT
      " does not implement method \"audio_valve_removed\"", self);
}

static void
kms_element_video_valve_removed_default (KmsElement * self, GstElement * valve)
{
  GST_WARNING
      ("Element class %" GST_PTR_FORMAT
      " does not implement method \"video_valve_removed\"", self);
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
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (kms_element_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (kms_element_release_pad);
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_factory));

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

  klass->audio_valve_added =
      GST_DEBUG_FUNCPTR (kms_element_audio_valve_added_default);
  klass->video_valve_added =
      GST_DEBUG_FUNCPTR (kms_element_video_valve_added_default);
  klass->audio_valve_removed =
      GST_DEBUG_FUNCPTR (kms_element_audio_valve_removed_default);
  klass->video_valve_removed =
      GST_DEBUG_FUNCPTR (kms_element_video_valve_removed_default);
  klass->sink_query = GST_DEBUG_FUNCPTR (kms_element_sink_query_default);

  /* set signals */
  element_signals[AGNOSTICBIN_ADDED] =
      g_signal_new ("agnosticbin-added",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsElementClass, agnosticbin_added), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (KmsElementPrivate));
}

static void
kms_element_init (KmsElement * element)
{
  g_rec_mutex_init (&element->mutex);

  element->priv = KMS_ELEMENT_GET_PRIVATE (element);

  g_object_set (G_OBJECT (element), "async-handling", TRUE, NULL);

  element->priv->accept_eos = DEFAULT_ACCEPT_EOS;
  element->priv->audio_pad_count = 0;
  element->priv->video_pad_count = 0;
  element->priv->audio_agnosticbin = NULL;
  element->priv->video_agnosticbin = NULL;

  element->priv->audio_valve = NULL;
  element->priv->video_valve = NULL;

  element->priv->base_time = GST_CLOCK_TIME_NONE;
  element->priv->base_clock = GST_CLOCK_TIME_NONE;
  g_mutex_init (&element->priv->sync_lock);
}

KmsElementPadType
kms_element_get_pad_type (KmsElement * self, GstPad * pad)
{
  KmsElementPadType type;
  GstPadTemplate *templ;

  g_return_val_if_fail (self != NULL, KMS_ELEMENT_PAD_TYPE_UNKNOWN);

  templ = gst_pad_get_pad_template (pad);

  if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), "audio_src_%u") ||
      templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), AUDIO_SINK_PAD)) {
    type = KMS_ELEMENT_PAD_TYPE_AUDIO;
  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), "video_src_%u") ||
      templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (self)), VIDEO_SINK_PAD)) {
    type = KMS_ELEMENT_PAD_TYPE_VIDEO;
  } else {
    type = KMS_ELEMENT_PAD_TYPE_UNKNOWN;
  }

  gst_object_unref (templ);

  return type;
}
