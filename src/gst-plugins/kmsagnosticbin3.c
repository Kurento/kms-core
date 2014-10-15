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

  guint src_pad_count;
  guint sink_pad_count;
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
  KMS_SRC_PAD_STATE_CONFIGURED,
  KMS_SRC_PAD_STATE_WAITING
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

  return pad;
}

static GstElement *
kms_agnostic_bin3_get_compatible_transcoder_tree (KmsAgnosticBin3 * self,
    const GstCaps * caps)
{
  GstElement *agnostic = NULL;
  GSList *l;

  KMS_AGNOSTIC_BIN3_LOCK (self);

  for (l = self->priv->agnosticbins; l != NULL; l = l->next) {
    /* TODO: Check all agnosticbin caps looking for a compatible caps */
  }

  KMS_AGNOSTIC_BIN3_UNLOCK (self);

  return agnostic;
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

static GstPad *
kms_agnostic_bin3_create_src_pad_with_caps (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstElement *element = NULL;
  KmsSrcPadData *paddata;
  gboolean ret = FALSE;
  GstPad *pad;

  element = kms_agnostic_bin3_get_compatible_transcoder_tree (self, caps);

  if (element != NULL) {
    /* TODO: Create ghost pad connected to the transcoder element */
    return NULL;
  }

  paddata = create_src_pad_data ();
  paddata->caps = gst_caps_copy (caps);

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

  g_object_set_data_full (G_OBJECT (pad), KMS_AGNOSTICBIN3_SRC_PAD_DATA,
      paddata, (GDestroyNotify) destroy_src_pad_data);

  kms_agnostic_bin3_append_pending_src_pad (self, pad);

  return pad;
}

static GstPad *
kms_agnostic_bin3_create_src_pad_without_caps (KmsAgnosticBin3 * self,
    GstPadTemplate * templ, const gchar * name)
{
  GstPad *pad;

  pad = kms_agnostic_bin3_create_new_src_pad (self, templ);
  kms_agnostic_bin3_append_pending_src_pad (self, pad);

  return pad;
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

  g_object_set (G_OBJECT (self), "async-handling", TRUE, NULL);
}

gboolean
kms_agnostic_bin3_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_AGNOSTIC_BIN3);
}
