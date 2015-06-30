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
#include "config.h"
#endif

#include <gst/gst.h>
#include "kmsdummysink.h"

#define PLUGIN_NAME "dummysink"

#define APPDATASINK "datasink"
#define APPAUDIOSINK "audiosink"
#define APPVIDEOSINK "videosink"

GST_DEBUG_CATEGORY_STATIC (kms_dummy_sink_debug_category);
#define GST_CAT_DEFAULT kms_dummy_sink_debug_category

#define KMS_DUMMY_SINK_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (           \
    (obj),                                \
    KMS_TYPE_DUMMY_SINK,                  \
    KmsDummySinkPrivate                   \
  )                                       \
)

struct _KmsDummySinkPrivate
{
  gboolean video;
  gboolean audio;
  gboolean data;
  GstElement *videoappsink;
  GstElement *audioappsink;
  GstElement *dataappsink;
};

G_DEFINE_TYPE_WITH_CODE (KmsDummySink, kms_dummy_sink,
    KMS_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (kms_dummy_sink_debug_category, PLUGIN_NAME,
        0, "debug category for kurento dummy plugin"));

/* Object properties */
enum
{
  PROP_0,
  PROP_DATA,
  PROP_AUDIO,
  PROP_VIDEO,
  N_PROPERTIES
};

#define DEFAULT_HTTP_ENDPOINT_START FALSE

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
kms_dummy_sink_add_sinkpad (KmsDummySink * self, KmsElementPadType type)
{
  GstElement **appsink;
  GstPad *sinkpad;
  gchar *name;

  switch (type) {
    case KMS_ELEMENT_PAD_TYPE_DATA:
      appsink = &self->priv->dataappsink;
      name = APPDATASINK;
      break;
    case KMS_ELEMENT_PAD_TYPE_AUDIO:
      appsink = &self->priv->audioappsink;
      name = APPAUDIOSINK;
      break;
    case KMS_ELEMENT_PAD_TYPE_VIDEO:
      appsink = &self->priv->videoappsink;
      name = APPVIDEOSINK;
      break;
    default:
      GST_ERROR_OBJECT (self, "Invalid pad type provided");
      return;
  }

  if (*appsink == NULL) {
    /* First time that appsink is created */
    *appsink = gst_element_factory_make ("fakesink", name);
    g_object_set (*appsink, "async", FALSE, "sync", FALSE, NULL);
    gst_bin_add (GST_BIN (self), *appsink);
    gst_element_sync_state_with_parent (*appsink);
  }

  sinkpad = gst_element_get_static_pad (*appsink, "sink");

  kms_element_connect_sink_target (KMS_ELEMENT (self), sinkpad, type);

  g_object_unref (sinkpad);
}

static void
kms_dummy_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsDummySink *self = KMS_DUMMY_SINK (object);
  gboolean val;

  KMS_ELEMENT_LOCK (KMS_ELEMENT (self));
  switch (property_id) {
    case PROP_DATA:
      val = g_value_get_boolean (value);
      if (val && !self->priv->data) {
        kms_dummy_sink_add_sinkpad (self, KMS_ELEMENT_PAD_TYPE_DATA);
      } else if (!val && self->priv->data) {
        kms_element_remove_sink_by_type (KMS_ELEMENT (self),
            KMS_ELEMENT_PAD_TYPE_DATA);
      } else {
        GST_DEBUG_OBJECT (self, "Operation without effect");
      }

      self->priv->data = val;
      break;
    case PROP_AUDIO:
      val = g_value_get_boolean (value);
      if (val && !self->priv->audio) {
        kms_dummy_sink_add_sinkpad (self, KMS_ELEMENT_PAD_TYPE_AUDIO);
      } else if (!val && self->priv->audio) {
        kms_element_remove_sink_by_type (KMS_ELEMENT (self),
            KMS_ELEMENT_PAD_TYPE_AUDIO);
      } else {
        GST_DEBUG_OBJECT (self, "Operation without effect");
      }

      self->priv->audio = val;
      break;
    case PROP_VIDEO:
      val = g_value_get_boolean (value);
      if (val && !self->priv->video) {
        kms_dummy_sink_add_sinkpad (self, KMS_ELEMENT_PAD_TYPE_VIDEO);
      } else if (!val && self->priv->video) {
        kms_element_remove_sink_by_type (KMS_ELEMENT (self),
            KMS_ELEMENT_PAD_TYPE_VIDEO);
      } else {
        GST_DEBUG_OBJECT (self, "Operation without effect");
      }

      self->priv->video = val;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  KMS_ELEMENT_UNLOCK (KMS_ELEMENT (self));
}

static void
kms_dummy_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  KmsDummySink *self = KMS_DUMMY_SINK (object);

  KMS_ELEMENT_LOCK (KMS_ELEMENT (self));
  switch (property_id) {
    case PROP_DATA:
      g_value_set_boolean (value, self->priv->data);
      break;
    case PROP_AUDIO:
      g_value_set_boolean (value, self->priv->audio);
      break;
    case PROP_VIDEO:
      g_value_set_boolean (value, self->priv->video);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  KMS_ELEMENT_UNLOCK (KMS_ELEMENT (self));
}

static void
kms_dummy_sink_class_init (KmsDummySinkClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = kms_dummy_sink_set_property;
  gobject_class->get_property = kms_dummy_sink_get_property;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "KmsDummySink",
      "Generic",
      "Dummy sink element",
      "Santiago Carot-Nemesio <sancane.kurento@gmail.com>");

  obj_properties[PROP_DATA] = g_param_spec_boolean ("data",
      "Data", "Provides data on TRUE", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  obj_properties[PROP_AUDIO] = g_param_spec_boolean ("audio",
      "Audio", "Provides audio on TRUE", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  obj_properties[PROP_VIDEO] = g_param_spec_boolean ("video",
      "Video", "Provides video on TRUE", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_properties (gobject_class,
      N_PROPERTIES, obj_properties);

  g_type_class_add_private (klass, sizeof (KmsDummySinkPrivate));
}

static void
kms_dummy_sink_init (KmsDummySink * self)
{
  self->priv = KMS_DUMMY_SINK_GET_PRIVATE (self);
}

gboolean
kms_dummy_sink_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_DUMMY_SINK);
}
