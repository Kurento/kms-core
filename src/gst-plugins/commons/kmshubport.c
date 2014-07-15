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
#include "config.h"
#endif

#include "kmshubport.h"
#include "kmsagnosticcaps.h"
#include "kmsutils.h"

#define PLUGIN_NAME "hubport"

#define KEY_VALVE_DATA "kms-hub-valve-data"

GST_DEBUG_CATEGORY_STATIC (kms_hub_port_debug_category);
#define GST_CAT_DEFAULT kms_hub_port_debug_category

#define KMS_HUB_PORT_GET_PRIVATE(obj) (         \
  G_TYPE_INSTANCE_GET_PRIVATE (                 \
    (obj),                                      \
    KMS_TYPE_HUB_PORT,                          \
    KmsHubPortPrivate                           \
  )                                             \
)

struct _KmsHubPortPrivate
{
  GstPad *audio_internal;
  GstPad *vieo_internal;
};

/* Pad templates */
static GstStaticPadTemplate hub_audio_sink_factory =
GST_STATIC_PAD_TEMPLATE (HUB_AUDIO_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS)
    );

static GstStaticPadTemplate hub_video_sink_factory =
GST_STATIC_PAD_TEMPLATE (HUB_VIDEO_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS)
    );

static GstStaticPadTemplate hub_audio_src_factory =
GST_STATIC_PAD_TEMPLATE (HUB_AUDIO_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_AUDIO_CAPS)
    );

static GstStaticPadTemplate hub_video_src_factory =
GST_STATIC_PAD_TEMPLATE (HUB_VIDEO_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_VIDEO_CAPS)
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsHubPort, kms_hub_port,
    KMS_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (kms_hub_port_debug_category, PLUGIN_NAME,
        0, "debug category for hubport element"));

static GstPad *
kms_hub_port_generate_sink_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps,
    GstElement * agnosticbin)
{
  GstPad *agnostic_pad, *pad;

  agnostic_pad = gst_element_get_static_pad (agnosticbin, "sink");
  pad = gst_ghost_pad_new_from_template (name, agnostic_pad, templ);
  g_object_unref (agnostic_pad);

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (element) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, TRUE);
  }

  if (gst_element_add_pad (element, pad))
    return pad;

  GST_ERROR_OBJECT (element, "Cannot add pad %" GST_PTR_FORMAT, pad);
  g_object_unref (pad);

  return NULL;
}

static GstPad *
kms_hub_port_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstElement *agnosticbin = NULL;

  if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), HUB_AUDIO_SINK_PAD)) {

    if (g_strcmp0 (name, HUB_AUDIO_SINK_PAD) != 0) {
      GST_ERROR_OBJECT (element,
          "Invalid pad name %s for template %" GST_PTR_FORMAT, name, templ);
      return NULL;
    }

    agnosticbin = kms_element_get_audio_agnosticbin (KMS_ELEMENT (element));
  } else if (templ ==
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), HUB_VIDEO_SINK_PAD)) {
    if (g_strcmp0 (name, HUB_VIDEO_SINK_PAD) != 0) {
      GST_ERROR_OBJECT (element,
          "Invalid pad name %s for template %" GST_PTR_FORMAT, name, templ);
      return NULL;
    }

    agnosticbin = kms_element_get_video_agnosticbin (KMS_ELEMENT (element));
  }

  if (agnosticbin != NULL)
    return kms_hub_port_generate_sink_pad (element, templ, name, caps,
        agnosticbin);

  return
      GST_ELEMENT_CLASS (kms_hub_port_parent_class)->request_new_pad
      (element, templ, name, caps);
}

static void
kms_hub_port_internal_src_pad_linked (GstPad * pad, GstPad * peer,
    gpointer data)
{
  GstElement *valve = g_object_get_data (G_OBJECT (pad), KEY_VALVE_DATA);

  kms_utils_set_valve_drop (valve, FALSE);
}

static void
kms_hub_port_internal_src_pad_unlinked (GstPad * pad, GstObject * parent)
{
  GstElement *valve = g_object_get_data (G_OBJECT (pad), KEY_VALVE_DATA);

  kms_utils_set_valve_drop (valve, TRUE);
}

static void
kms_hub_port_valve_added (KmsElement * self, GstElement * valve,
    GstPadTemplate * templ, const gchar * pad_name)
{
  GstPad *src = gst_element_get_static_pad (valve, "src");
  GstPad *internal_src = gst_ghost_pad_new_from_template (pad_name, src, templ);

  g_object_set_data_full (G_OBJECT (internal_src), KEY_VALVE_DATA,
      g_object_ref (valve), g_object_unref);

  g_signal_connect (internal_src, "linked",
      G_CALLBACK (kms_hub_port_internal_src_pad_linked), NULL);
  internal_src->unlinkfunc =
      GST_DEBUG_FUNCPTR (kms_hub_port_internal_src_pad_unlinked);

  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED) {
    gst_pad_set_active (internal_src, TRUE);
  }

  gst_element_add_pad (GST_ELEMENT (self), internal_src);
  g_object_unref (src);
}

static void
kms_hub_port_audio_valve_added (KmsElement * self, GstElement * valve)
{
  GstPadTemplate *templ = gst_static_pad_template_get (&hub_audio_src_factory);

  kms_hub_port_valve_added (self, valve, templ, HUB_AUDIO_SRC_PAD);
  g_object_unref (templ);
}

static void
kms_hub_port_video_valve_added (KmsElement * self, GstElement * valve)
{
  GstPadTemplate *templ = gst_static_pad_template_get (&hub_video_src_factory);

  kms_hub_port_valve_added (self, valve, templ, HUB_VIDEO_SRC_PAD);
  g_object_unref (templ);
}

static void
kms_hub_port_valve_removed (KmsElement * self, GstElement * valve,
    const gchar * pad_name)
{
  GstPad *src = gst_element_get_static_pad (GST_ELEMENT (self), pad_name);
  GstPad *peer = gst_pad_get_peer (src);

  if (peer != NULL) {
    gst_pad_unlink (src, peer);
    g_object_unref (peer);
  }

  gst_element_remove_pad (GST_ELEMENT (self), src);
}

static void
kms_hub_port_audio_valve_removed (KmsElement * self, GstElement * valve)
{
  kms_hub_port_valve_removed (self, valve, HUB_AUDIO_SRC_PAD);
}

static void
kms_hub_port_video_valve_removed (KmsElement * self, GstElement * valve)
{
  kms_hub_port_valve_removed (self, valve, HUB_VIDEO_SRC_PAD);
}

static void
kms_hub_port_dispose (GObject * object)
{
  G_OBJECT_CLASS (kms_hub_port_parent_class)->dispose (object);
}

static void
kms_hub_port_finalize (GObject * object)
{
  G_OBJECT_CLASS (kms_hub_port_parent_class)->finalize (object);
}

static void
kms_hub_port_class_init (KmsHubPortClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  KmsElementClass *kms_element_class;

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "HubPort", "Generic", "Kurento plugin for mixer connection",
      "Jose Antonio Santos Cadenas <santoscadenas@gmail.com>");

  gobject_class->dispose = kms_hub_port_dispose;
  gobject_class->finalize = kms_hub_port_finalize;

  kms_element_class = KMS_ELEMENT_CLASS (klass);

  kms_element_class->audio_valve_added =
      GST_DEBUG_FUNCPTR (kms_hub_port_audio_valve_added);
  kms_element_class->video_valve_added =
      GST_DEBUG_FUNCPTR (kms_hub_port_video_valve_added);
  kms_element_class->audio_valve_removed =
      GST_DEBUG_FUNCPTR (kms_hub_port_audio_valve_removed);
  kms_element_class->video_valve_removed =
      GST_DEBUG_FUNCPTR (kms_hub_port_video_valve_removed);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (kms_hub_port_request_new_pad);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&hub_audio_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&hub_video_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&hub_audio_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&hub_video_sink_factory));

  /* Registers a private structure for the instantiatable type */
  g_type_class_add_private (klass, sizeof (KmsHubPortPrivate));
}

static void
kms_hub_port_init (KmsHubPort * self)
{
  self->priv = KMS_HUB_PORT_GET_PRIVATE (self);
}

gboolean
kms_hub_port_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_HUB_PORT);
}
