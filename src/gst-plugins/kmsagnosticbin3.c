/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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

#include "kmsagnosticbin3.h"
#include "kmsagnosticcaps.h"
#include "kms-core-marshal.h"
#include "kmsutils.h"

#define PLUGIN_NAME "agnosticbin3"

#define KMS_AGNOSTICBIN3_SRC_PAD_DATA "kms-agnosticbin3-src-pad-data"

#define GST_CAT_DEFAULT kms_agnostic_bin3_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_agnostic_bin3_parent_class parent_class
G_DEFINE_TYPE (KmsAgnosticBin3, kms_agnostic_bin3, GST_TYPE_BIN);

#define KMS_AGNOSTIC_BIN3_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (              \
    (obj),                                   \
    KMS_TYPE_AGNOSTIC_BIN3,                  \
    KmsAgnosticBin3Private                   \
  )                                          \
)

struct _KmsAgnosticBin3Private
{
  GRecMutex mutex;
  GSList *agnosticbins;
  GHashTable *sinkcaps;

  guint src_pad_count;
  guint sink_pad_count;

  guint last;
};

#define KMS_AGNOSTIC_BIN3_LOCK(obj) (                        \
  g_rec_mutex_lock (&KMS_AGNOSTIC_BIN3 (obj)->priv->mutex)   \
)

#define KMS_AGNOSTIC_BIN3_UNLOCK(obj) (                      \
  g_rec_mutex_unlock (&KMS_AGNOSTIC_BIN3 (obj)->priv->mutex) \
)

typedef enum
{
  KMS_SRC_PAD_STATE_UNCONFIGURED,
  KMS_SRC_PAD_STATE_CONFIGURING,
  KMS_SRC_PAD_STATE_CONFIGURED,
  KMS_SRC_PAD_STATE_WAITING,
  KMS_SRC_PAD_STATE_LINKED
} KmsSrcPadState;

typedef struct _KmsSrcPadData
{
  GMutex mutex;
  KmsSrcPadState state;
  GstCaps *caps;
} KmsSrcPadData;

/* Object signals */
enum
{
  SIGNAL_CAPS,
  LAST_SIGNAL
};

static guint agnosticbin3_signals[LAST_SIGNAL] = { 0 };

#define AGNOSTICBIN3_SINK_PAD_PREFIX  "sink_"
#define AGNOSTICBIN3_SRC_PAD_PREFIX  "src_"

#define AGNOSTICBIN3_SINK_PAD AGNOSTICBIN3_SINK_PAD_PREFIX "%u"
#define AGNOSTICBIN3_SRC_PAD AGNOSTICBIN3_SRC_PAD_PREFIX "%u"

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE (AGNOSTICBIN3_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_CAPS_CAPS)
    );

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (AGNOSTICBIN3_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_CAPS_CAPS)
    );

static KmsSrcPadData *
create_src_pad_data ()
{
  KmsSrcPadData *data;

  data = g_slice_new0 (KmsSrcPadData);
  g_mutex_init (&data->mutex);
  data->state = KMS_SRC_PAD_STATE_UNCONFIGURED;

  return data;
}

static void
destroy_src_pad_data (KmsSrcPadData * data)
{
  if (data->caps != NULL) {
    gst_caps_unref (data->caps);
  }

  g_mutex_clear (&data->mutex);
  g_slice_free (KmsSrcPadData, data);
}

static const gchar *
pad_state2string (KmsSrcPadState state)
{
  switch (state) {
    case KMS_SRC_PAD_STATE_UNCONFIGURED:
      return "UNCONFIGURED";
    case KMS_SRC_PAD_STATE_CONFIGURING:
      return "CONFIGURING";
    case KMS_SRC_PAD_STATE_CONFIGURED:
      return "CONFIGURED";
    case KMS_SRC_PAD_STATE_WAITING:
      return "WAITING";
    case KMS_SRC_PAD_STATE_LINKED:
      return "LINKED";
    default:
      return NULL;
  }
}

static void
kms_agnostic_bin3_append_pending_src_pad (KmsAgnosticBin3 * self, GstPad * pad)
{
  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, TRUE);
  }

  gst_element_add_pad (GST_ELEMENT (self), pad);
}

static GstElement *
get_transcoder_connected_to_sinkpad (GstPad * pad)
{
  GstElement *transcoder;
  GstProxyPad *proxypad;
  GstPad *sinkpad;

  proxypad = gst_proxy_pad_get_internal (GST_PROXY_PAD (pad));
  sinkpad = gst_pad_get_peer (GST_PAD (proxypad));
  transcoder = gst_pad_get_parent_element (sinkpad);

  g_object_unref (proxypad);
  g_object_unref (sinkpad);

  return transcoder;
}

static void
connect_srcpad_to_encoder (GstPad * srcpad, GstPad * sinkpad)
{
  GstCaps *current_caps = NULL;
  GstElement *transcoder;
  KmsAgnosticBin3 *self;
  KmsSrcPadData *data;
  GstPad *target;
  gboolean transcode;

  data = g_object_get_data (G_OBJECT (srcpad), KMS_AGNOSTICBIN3_SRC_PAD_DATA);
  if (data == NULL) {
    GST_ERROR_OBJECT (srcpad, "No configuration data");
    return;
  }

  self = KMS_AGNOSTIC_BIN3 (gst_pad_get_parent_element (sinkpad));
  if (self == NULL) {
    GST_ERROR_OBJECT (sinkpad, "No parent object");
    return;
  }

  g_mutex_lock (&data->mutex);

  GST_DEBUG_OBJECT (srcpad, "state: %s", pad_state2string (data->state));

  if (data->state == KMS_SRC_PAD_STATE_LINKED) {
    goto end;
  }

  current_caps = g_hash_table_lookup (self->priv->sinkcaps, sinkpad);

  switch (data->state) {
    case KMS_SRC_PAD_STATE_UNCONFIGURED:{
      GstCaps *caps;

      caps = gst_pad_peer_query_caps (srcpad, current_caps);
      transcode = gst_caps_is_empty (caps);
      gst_caps_unref (caps);

      break;
    }
    case KMS_SRC_PAD_STATE_CONFIGURED:
      transcode = !gst_caps_can_intersect (data->caps, current_caps);
      break;
    default:
      GST_DEBUG_OBJECT (srcpad, "TODO: Operate in %s",
          pad_state2string (data->state));
      goto end;
  }

  if (transcode) {
    /* TODO: Ask to see if anyone upstream supports this caps */
    GST_DEBUG_OBJECT (srcpad, "Connection will require transcoding");
  }

  /* We can connect this pad to te agnosticbin without transcoding */

  transcoder = get_transcoder_connected_to_sinkpad (sinkpad);
  if (transcoder == NULL) {
    GST_ERROR_OBJECT (sinkpad, "No transcoder connected");
    goto end;
  }

  target = gst_element_get_request_pad (transcoder, "src_%u");
  g_object_unref (transcoder);

  GST_DEBUG_OBJECT (srcpad, "Setting target %" GST_PTR_FORMAT, target);

  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (srcpad), target)) {
    GST_ERROR_OBJECT (srcpad, "Can not set target pad");
    gst_element_release_request_pad (transcoder, target);
  } else {
    data->state = KMS_SRC_PAD_STATE_LINKED;
  }

  g_object_unref (target);

end:

  g_mutex_unlock (&data->mutex);
  g_object_unref (self);
}

static void
link_pending_src_pads (GstPad * srcpad, GstPad * sinkpad)
{
  KmsAgnosticBin3 *self =
      KMS_AGNOSTIC_BIN3 (gst_pad_get_parent_element (sinkpad));

  if (self == NULL) {
    GST_ERROR_OBJECT (sinkpad, "No parent object");
    return;
  }

  if (!gst_pad_is_linked (srcpad)) {
    GST_DEBUG_OBJECT (self, "Unlinked pad %" GST_PTR_FORMAT, srcpad);
  } else {
    connect_srcpad_to_encoder (srcpad, sinkpad);
  }

  g_object_unref (self);
}

/* Gets the transcoder that can manage these caps or NULL. [Transfer full] */
static GstElement *
kms_agnosticbin3_get_element_for_transcoding (KmsAgnosticBin3 * self)
{
  GstElement *transcoder;
  guint index, len;
  GSList *l;

  index = (guint) g_atomic_int_add (&self->priv->last, 1);

  KMS_AGNOSTIC_BIN3_LOCK (self);

  len = g_slist_length (self->priv->agnosticbins);
  index %= len;
  l = g_slist_nth (self->priv->agnosticbins, index);
  transcoder = l->data;

  KMS_AGNOSTIC_BIN3_UNLOCK (self);

  if (transcoder != NULL)
    g_object_ref (transcoder);

  return transcoder;
}

static GstPadProbeReturn
kms_agnostic_bin3_sink_caps_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer data)
{
  GstEvent *event = gst_pad_probe_info_get_event (info);
  KmsAgnosticBin3 *self;
  GstCaps *caps, *current_caps;

  if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
    return GST_PAD_PROBE_OK;
  }

  GST_DEBUG_OBJECT (pad, "Event received %" GST_PTR_FORMAT, event);
  gst_event_parse_caps (event, &caps);

  self = KMS_AGNOSTIC_BIN3 (data);

  if (caps == NULL) {
    GST_ERROR_OBJECT (self, "Unexpected NULL caps");
    return GST_PAD_PROBE_OK;
  }

  KMS_AGNOSTIC_BIN3_LOCK (self);

  current_caps = g_hash_table_lookup (self->priv->sinkcaps, pad);

  if (current_caps == NULL) {
    GST_DEBUG_OBJECT (pad, "Current input caps %" GST_PTR_FORMAT, caps);
    g_hash_table_insert (self->priv->sinkcaps, pad, gst_caps_copy (caps));
  } else if (gst_caps_is_equal (caps, current_caps)) {
    GST_DEBUG_OBJECT (pad, "Caps already set %" GST_PTR_FORMAT, caps);
    goto end;
  } else {
    GST_WARNING_OBJECT (pad, "TODO: Input caps changed %" GST_PTR_FORMAT, caps);
    goto end;
  }

  kms_element_for_each_src_pad (GST_ELEMENT (self),
      (KmsPadIterationAction) link_pending_src_pads, pad);

end:

  KMS_AGNOSTIC_BIN3_UNLOCK (self);

  return GST_PAD_PROBE_OK;
}

static GstPad *
kms_agnostic_bin3_request_sink_pad (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstElement *agnosticbin;
  GstPad *target, *pad = NULL;
  gchar *padname;

  agnosticbin = gst_element_factory_make ("agnosticbin", NULL);

  gst_bin_add (GST_BIN (self), agnosticbin);
  gst_element_sync_state_with_parent (agnosticbin);

  target = gst_element_get_static_pad (agnosticbin, "sink");

  padname = g_strdup_printf (AGNOSTICBIN3_SINK_PAD,
      g_atomic_int_add (&self->priv->sink_pad_count, 1));

  pad = gst_ghost_pad_new (padname, target);

  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, TRUE);
  }

  gst_element_add_pad (GST_ELEMENT (self), pad);

  g_free (padname);
  g_object_unref (target);

  KMS_AGNOSTIC_BIN3_LOCK (self);

  self->priv->agnosticbins = g_slist_prepend (self->priv->agnosticbins,
      agnosticbin);

  KMS_AGNOSTIC_BIN3_UNLOCK (self);

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      kms_agnostic_bin3_sink_caps_probe, self, NULL);

  return pad;
}

/* Gets the transcoder that can manage these caps or NULL. [Transfer full] */
/* Call this function with mutex held */
static GstElement *
kms_agnostic_bin3_get_compatible_transcoder_tree (KmsAgnosticBin3 * self,
    const GstCaps * caps)
{
  GHashTableIter iter;
  gpointer key, value;

  /* Check out all agnosticbin's sink capabilities */

  g_hash_table_iter_init (&iter, self->priv->sinkcaps);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GstPad *sinkpad = GST_PAD (key);
    GstCaps *sinkcaps = GST_CAPS (value);

    GST_INFO_OBJECT (sinkpad, "caps %" GST_PTR_FORMAT, sinkcaps);
    if (gst_caps_can_intersect (caps, sinkcaps)) {
      return get_transcoder_connected_to_sinkpad (sinkpad);
    }
  }

  /* TODO: Check all agnostic's src pads looking for compatible  */
  /* capabilities, iIf we find them it means that this element is */
  /* already transcoding. */

  return NULL;
}

static GstPad *
kms_agnostic_bin3_create_new_src_pad (KmsAgnosticBin3 * self,
    GstPadTemplate * templ)
{
  gchar *padname;
  GstPad *pad;

  padname = g_strdup_printf (AGNOSTICBIN3_SRC_PAD,
      g_atomic_int_add (&self->priv->src_pad_count, 1));
  pad = gst_ghost_pad_new_no_target_from_template (padname, templ);
  g_free (padname);

  return pad;
}

static GstPad *
kms_agnostic_bin3_create_src_pad_with_transcodification (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const GstCaps * caps)
{
  GstPad *pad;

  /* TODO: Check out if an agnosticbin with our caps has been created.  */
  /* If so, create a ghost pad connected to it. If not, create an empty */
  /* ghost pad that we will connect when we have input stream */

  pad = kms_agnostic_bin3_create_new_src_pad (self, templ);

  return pad;
}

static gboolean
set_transcoder_src_target_pad (GstGhostPad * pad, GstElement * transcoder)
{
  GstPad *target;
  gboolean ret;

  target = gst_element_get_request_pad (transcoder, "src_%u");

  if (!(ret = gst_ghost_pad_set_target (GST_GHOST_PAD (pad), target))) {
    GST_ERROR_OBJECT (pad, "Can not set target pad");
    gst_element_release_request_pad (transcoder, target);
  } else {
    GST_LOG_OBJECT (pad, "Set target %" GST_PTR_FORMAT, target);
  }

  g_object_unref (target);

  return ret;
}

static GstPad *
kms_agnostic_bin3_create_src_pad_with_caps (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstElement *element = NULL;
  KmsSrcPadData *paddata;
  gboolean ret = FALSE;
  GstPad *pad;

  paddata = create_src_pad_data ();
  paddata->caps = gst_caps_copy (caps);

  KMS_AGNOSTIC_BIN3_LOCK (self);

  element = kms_agnostic_bin3_get_compatible_transcoder_tree (self, caps);

  if (element != NULL) {
    KMS_AGNOSTIC_BIN3_UNLOCK (self);

    /* Create ghost pad connected to the transcoder element */
    pad = kms_agnostic_bin3_create_new_src_pad (self, templ);
    if (set_transcoder_src_target_pad (GST_GHOST_PAD (pad), element)) {
      paddata->state = KMS_SRC_PAD_STATE_LINKED;
    } else {
      /* We got caps but we could not link with this agnostic */
      paddata->state = KMS_SRC_PAD_STATE_CONFIGURED;
    }

    g_object_unref (element);
    goto end;
  }

  KMS_AGNOSTIC_BIN3_UNLOCK (self);

  /* Trigger caps signal */
  g_signal_emit (G_OBJECT (self), agnosticbin3_signals[SIGNAL_CAPS], 0, caps,
      &ret);

  if (ret) {
    /* Someone upstream supports these caps */
    paddata->state = KMS_SRC_PAD_STATE_WAITING;
    pad = kms_agnostic_bin3_create_new_src_pad (self, templ);
  } else {
    /* Transcode will be done in any available agnosticbin */
    paddata->state = KMS_SRC_PAD_STATE_CONFIGURED;
    pad = kms_agnostic_bin3_create_src_pad_with_transcodification (self, templ,
        caps);
  }

end:
  g_object_set_data_full (G_OBJECT (pad), KMS_AGNOSTICBIN3_SRC_PAD_DATA,
      paddata, (GDestroyNotify) destroy_src_pad_data);

  kms_agnostic_bin3_append_pending_src_pad (self, pad);

  return pad;
}

static GstPad *
kms_agnostic_bin3_create_src_pad_without_caps (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name)
{
  KmsSrcPadData *paddata;
  GstPad *pad;

  paddata = create_src_pad_data ();
  pad = kms_agnostic_bin3_create_new_src_pad (self, templ);
  g_object_set_data_full (G_OBJECT (pad), KMS_AGNOSTICBIN3_SRC_PAD_DATA,
      paddata, (GDestroyNotify) destroy_src_pad_data);

  kms_agnostic_bin3_append_pending_src_pad (self, pad);

  return pad;
}

static void
kms_agnostic_bin3_src_pad_linked (GstPad * pad, GstPad * peer,
    gpointer user_data)
{
  KmsAgnosticBin3 *self = KMS_AGNOSTIC_BIN3 (user_data);
  KmsSrcPadState new_state = KMS_SRC_PAD_STATE_UNCONFIGURED;
  GstElement *element;
  KmsSrcPadData *data;
  GstCaps *caps = NULL;
  gboolean ret;

  data = g_object_get_data (G_OBJECT (pad), KMS_AGNOSTICBIN3_SRC_PAD_DATA);
  if (data == NULL) {
    GST_ERROR_OBJECT (pad, "No configuration data");
    return;
  }

  g_mutex_lock (&data->mutex);

  if (data->state != KMS_SRC_PAD_STATE_UNCONFIGURED) {
    GST_DEBUG_OBJECT (pad, "Already configured");
    g_mutex_unlock (&data->mutex);
    return;
  }

  data->state = KMS_SRC_PAD_STATE_CONFIGURING;

  g_mutex_unlock (&data->mutex);

  caps = gst_pad_query_caps (peer, NULL);

  KMS_AGNOSTIC_BIN3_LOCK (self);

  if (g_slist_length (self->priv->agnosticbins) == 0) {
    GST_DEBUG_OBJECT (self, "No transcoders available");
    new_state = KMS_SRC_PAD_STATE_UNCONFIGURED;
    goto change_state;
  }

  element = kms_agnostic_bin3_get_compatible_transcoder_tree (self, caps);

  if (element != NULL) {
    GST_DEBUG_OBJECT (pad, "Connected without transcoding");
    goto connect_transcoder;
  }

  g_signal_emit (G_OBJECT (self), agnosticbin3_signals[SIGNAL_CAPS], 0, caps,
      &ret);
  if (ret) {
    /* Someone upstream supports these caps, there is not need to transcode */
    new_state = KMS_SRC_PAD_STATE_WAITING;
    goto change_state;
  }

  /* Get any available transcoder. Round robin will be ussed */
  element = kms_agnosticbin3_get_element_for_transcoding (self);
  if (element == NULL) {
    GST_ERROR_OBJECT (pad, "Can not connect to any encoder");
    goto change_state;
  }

  GST_DEBUG_OBJECT (pad, "Connected transcoding");

connect_transcoder:
  {
    if (set_transcoder_src_target_pad (GST_GHOST_PAD (pad), element)) {
      new_state = KMS_SRC_PAD_STATE_LINKED;
    } else {
      new_state = KMS_SRC_PAD_STATE_UNCONFIGURED;
    }

    g_object_unref (element);
  }
change_state:
  {
    g_mutex_lock (&data->mutex);
    data->state = new_state;
    g_mutex_unlock (&data->mutex);

    KMS_AGNOSTIC_BIN3_UNLOCK (self);

    if (caps != NULL) {
      gst_caps_unref (caps);
    }
  }
}

static GstPad *
kms_agnostic_bin3_request_src_pad (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad;

  if (caps != NULL) {
    pad = kms_agnostic_bin3_create_src_pad_with_caps (self, templ, name, caps);
  } else {
    pad = kms_agnostic_bin3_create_src_pad_without_caps (self, templ, name);
  }

  if (pad != NULL) {
    g_signal_connect (pad, "linked",
        G_CALLBACK (kms_agnostic_bin3_src_pad_linked), self);
  }

  return pad;
}

static GstPad *
kms_agnostic_bin3_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  KmsAgnosticBin3 *self = KMS_AGNOSTIC_BIN3 (element);

  if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), AGNOSTICBIN3_SINK_PAD)) {
    return kms_agnostic_bin3_request_sink_pad (self, templ, name, caps);
  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), AGNOSTICBIN3_SRC_PAD)) {
    return kms_agnostic_bin3_request_src_pad (self, templ, name, caps);
  } else {
    GST_ERROR_OBJECT (element, "Unsupported pad template %" GST_PTR_FORMAT,
        templ);
    return NULL;
  }
}

static void
kms_agnostic_bin3_release_pad (GstElement * element, GstPad * pad)
{
  /* TODO: */
  GST_DEBUG_OBJECT (element, "Release pad");
}

static void
kms_agnostic_bin3_finalize (GObject * object)
{
  KmsAgnosticBin3 *self = KMS_AGNOSTIC_BIN3 (object);

  g_slist_free (self->priv->agnosticbins);
  g_hash_table_unref (self->priv->sinkcaps);
  g_rec_mutex_clear (&self->priv->mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_agnostic_bin3_class_init (KmsAgnosticBin3Class * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_agnostic_bin3_finalize;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "Agnostic connector 3rd version",
      "Generic/Bin/Connector",
      "Automatically encodes/decodes media to match sink and source pads caps",
      "Santiago Carot-Nemesio <sancane_at_gmail_dot_com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (kms_agnostic_bin3_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (kms_agnostic_bin3_release_pad);

  /* set signals */
  agnosticbin3_signals[SIGNAL_CAPS] =
      g_signal_new ("caps",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsAgnosticBin3Class, caps_signal), NULL, NULL,
      __kms_core_marshal_BOOLEAN__BOXED, G_TYPE_BOOLEAN, 1, GST_TYPE_CAPS);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  g_type_class_add_private (klass, sizeof (KmsAgnosticBin3Private));
}

static void
kms_agnostic_bin3_init (KmsAgnosticBin3 * self)
{
  self->priv = KMS_AGNOSTIC_BIN3_GET_PRIVATE (self);
  g_rec_mutex_init (&self->priv->mutex);
  self->priv->sinkcaps = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gst_caps_unref);

  g_object_set (G_OBJECT (self), "async-handling", TRUE, NULL);
}

gboolean
kms_agnostic_bin3_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_AGNOSTIC_BIN3);
}
