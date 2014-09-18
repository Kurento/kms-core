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

#include "kmsbufferinjector.h"

#define PLUGIN_NAME "bufferinjector"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (kms_buffer_injector_debug);
#define GST_CAT_DEFAULT kms_buffer_injector_debug

G_DEFINE_TYPE_WITH_CODE (KmsBufferInjector, kms_buffer_injector,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (kms_buffer_injector_debug,
        PLUGIN_NAME, 0, "debug category for " PLUGIN_NAME " element"));

#define KMS_BUFFER_INJECTOR_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (              \
    (obj),                                   \
    KMS_TYPE_BUFFER_INJECTOR,                \
    KmsBufferInjectorPrivate                 \
  )                                          \
)

#define KMS_BUFFER_INJECTOR_GET_COND(obj) (       \
  &KMS_BUFFER_INJECTOR (obj)->priv->thread_cond   \
)

#define KMS_BUFFER_INJECTOR_LOCK(obj) (                           \
  g_mutex_lock (&KMS_BUFFER_INJECTOR (obj)->priv->thread_mutex)   \
)

#define KMS_BUFFER_INJECTOR_UNLOCK(obj) (                         \
  g_mutex_unlock (&KMS_BUFFER_INJECTOR (obj)->priv->thread_mutex) \
)

struct _KmsBufferInjectorPrivate
{
  GMutex thread_mutex;
  GstPad *sinkpad;
  GstPad *srcpad;
};

static GstFlowReturn
kms_buffer_injector_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  return GST_FLOW_OK;
}

static gboolean
kms_buffer_injector_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      GST_DEBUG ("CAPS RECEIVED");
      break;
    default:
      break;
  }

  return TRUE;
}

static void
kms_buffer_injector_init (KmsBufferInjector * self)
{
  self->priv = KMS_BUFFER_INJECTOR_GET_PRIVATE (self);

  self->priv->sinkpad =
      gst_pad_new_from_static_template (&sinktemplate, "sink");

  gst_pad_set_chain_function (self->priv->sinkpad, kms_buffer_injector_chain);
  gst_pad_set_event_function (self->priv->sinkpad,
      kms_buffer_injector_handle_sink_event);
  GST_PAD_SET_PROXY_CAPS (self->priv->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->priv->sinkpad);

  self->priv->srcpad = gst_pad_new_from_static_template (&srctemplate, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->priv->srcpad);

  g_mutex_init (&self->priv->thread_mutex);
}

static void
kms_buffer_injector_class_init (KmsBufferInjectorClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "Buffer injector",
      "Generic/Bin/Connector",
      "Injects buffers in pipeline if the frame rate is not enough",
      "David Fernández López <d.fernandezlop@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  GST_DEBUG_REGISTER_FUNCPTR (kms_buffer_injector_chain);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  g_type_class_add_private (klass, sizeof (KmsBufferInjectorPrivate));
}

gboolean
kms_buffer_injector_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_BUFFER_INJECTOR);
}
