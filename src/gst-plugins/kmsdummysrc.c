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
#include "kmsdummysrc.h"

#define PLUGIN_NAME "dummysrc"

GST_DEBUG_CATEGORY_STATIC (kms_dummy_src_debug_category);
#define GST_CAT_DEFAULT kms_dummy_src_debug_category

#define KMS_DUMMY_SRC_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (           \
    (obj),                                \
    KMS_TYPE_DUMMY_SRC,                   \
    KmsDummySrcPrivate                    \
  )                                       \
)

struct _KmsDummySrcPrivate
{
  gboolean video;
  gboolean audio;
  GstElement *videoappsrc;
  GstElement *audioappsrc;
};

G_DEFINE_TYPE_WITH_CODE (KmsDummySrc, kms_dummy_src,
    KMS_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (kms_dummy_src_debug_category, PLUGIN_NAME,
        0, "debug category for kurento dummy plugin"));

/* Object properties */
enum
{
  PROP_0,
  PROP_AUDIO,
  PROP_VIDEO,
  N_PROPERTIES
};

#define DEFAULT_HTTP_ENDPOINT_START FALSE

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
kms_dummy_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsDummySrc *self = KMS_DUMMY_SRC (object);

  KMS_ELEMENT_LOCK (KMS_ELEMENT (self));
  switch (property_id) {
    case PROP_AUDIO:
      self->priv->audio = g_value_get_boolean (value);

      if (self->priv->audio && self->priv->audioappsrc == NULL) {
        GstElement *agnosticbin;

        GST_DEBUG_OBJECT (self, "Creating audio stream");
        agnosticbin = kms_element_get_audio_agnosticbin (KMS_ELEMENT (self));

        self->priv->audioappsrc =
            gst_element_factory_make ("audiotestsrc", NULL);
        g_object_set (G_OBJECT (self->priv->audioappsrc), "is-live", TRUE,
            NULL);
        gst_bin_add (GST_BIN (self), self->priv->audioappsrc);
        gst_element_link_pads (self->priv->audioappsrc, "src", agnosticbin,
            "sink");
        gst_element_sync_state_with_parent (self->priv->audioappsrc);
      }
      break;
    case PROP_VIDEO:
      self->priv->video = g_value_get_boolean (value);

      if (self->priv->video && self->priv->videoappsrc == NULL) {
        GstElement *agnosticbin;

        GST_DEBUG_OBJECT (self, "Creating video stream");
        agnosticbin = kms_element_get_video_agnosticbin (KMS_ELEMENT (self));

        self->priv->videoappsrc =
            gst_element_factory_make ("videotestsrc", NULL);
        g_object_set (G_OBJECT (self->priv->videoappsrc), "is-live", TRUE,
            NULL);
        gst_bin_add (GST_BIN (self), self->priv->videoappsrc);
        gst_element_link_pads (self->priv->videoappsrc, "src", agnosticbin,
            "sink");
        gst_element_sync_state_with_parent (self->priv->videoappsrc);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  KMS_ELEMENT_UNLOCK (KMS_ELEMENT (self));
}

static void
kms_dummy_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  KmsDummySrc *self = KMS_DUMMY_SRC (object);

  KMS_ELEMENT_LOCK (KMS_ELEMENT (self));
  switch (property_id) {
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
kms_dummy_src_class_init (KmsDummySrcClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = kms_dummy_src_set_property;
  gobject_class->get_property = kms_dummy_src_get_property;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "KmsDummySrc",
      "Generic",
      "Dummy src element",
      "Santiago Carot-Nemesio <sancane.kurento@gmail.com>");

  obj_properties[PROP_AUDIO] = g_param_spec_boolean ("audio",
      "Audio", "Provides audio on TRUE", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  obj_properties[PROP_VIDEO] = g_param_spec_boolean ("video",
      "Video", "Provides video on TRUE", FALSE,
      (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_properties (gobject_class,
      N_PROPERTIES, obj_properties);

  g_type_class_add_private (klass, sizeof (KmsDummySrcPrivate));
}

static void
kms_dummy_src_init (KmsDummySrc * self)
{
  self->priv = KMS_DUMMY_SRC_GET_PRIVATE (self);
}

gboolean
kms_dummy_src_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_DUMMY_SRC);
}
