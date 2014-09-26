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
#define DEFAULT_WAITING_TIME (((GST_SECOND) / 15) / 1000)

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
#define kms_buffer_injector_parent_class parent_class

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
  g_rec_mutex_lock (&KMS_BUFFER_INJECTOR (obj)->priv->thread_mutex)   \
)

#define KMS_BUFFER_INJECTOR_UNLOCK(obj) (                         \
  g_rec_mutex_unlock (&KMS_BUFFER_INJECTOR (obj)->priv->thread_mutex) \
)

typedef enum
{
  VIDEO,
  AUDIO
} MediaType;

struct _KmsBufferInjectorPrivate
{
  GRecMutex thread_mutex;
  GstPad *sinkpad;
  GstPad *srcpad;
  gboolean configured;
  gboolean still_waiting;
  MediaType type;
  GstBuffer *previous_buffer;
  GMutex mutex_generate;
  GCond cond_generate;
  gint64 wait_time;
};

static gint64
kms_buffer_injector_get_wait_time (KmsBufferInjector * self)
{
  gint64 aux;

  KMS_BUFFER_INJECTOR_LOCK (self);
  aux = self->priv->wait_time;
  KMS_BUFFER_INJECTOR_UNLOCK (self);

  return aux;
}

static void
kms_buffer_injector_generate_buffers (KmsBufferInjector * self)
{
  gint64 end_time;

  KMS_BUFFER_INJECTOR_LOCK (self);
  if ((!self->priv->configured) || (self->priv->previous_buffer == NULL)) {
    KMS_BUFFER_INJECTOR_UNLOCK (self);
    return;
  }
  KMS_BUFFER_INJECTOR_UNLOCK (self);

  end_time =
      g_get_monotonic_time () + (2 * kms_buffer_injector_get_wait_time (self));

  g_mutex_lock (&self->priv->mutex_generate);

  if (!self->priv->still_waiting) {
    g_mutex_unlock (&self->priv->mutex_generate);
    return;
  }

  if (!g_cond_wait_until (&self->priv->cond_generate,
          &self->priv->mutex_generate, end_time)) {
    GstBuffer *copy;

    //timeout reached, it is necessary to inject a new buffer
    GST_DEBUG_OBJECT (self->priv->srcpad, "Injecting buffer");
    KMS_BUFFER_INJECTOR_LOCK (self);
    copy = gst_buffer_make_writable (self->priv->previous_buffer);
    if (GST_BUFFER_DTS_IS_VALID (copy)) {
      copy->dts = copy->dts + self->priv->wait_time;
    }
    if (GST_BUFFER_PTS_IS_VALID (copy)) {
      copy->pts = copy->pts + self->priv->wait_time;
    }

    GST_BUFFER_FLAG_SET (copy, GST_BUFFER_FLAG_GAP);
    GST_BUFFER_FLAG_SET (copy, GST_BUFFER_FLAG_DROPPABLE);
    KMS_BUFFER_INJECTOR_UNLOCK (self);
    gst_pad_push (self->priv->srcpad, copy);
  }
  g_mutex_unlock (&self->priv->mutex_generate);
}

static gboolean
kms_buffer_injector_config (KmsBufferInjector * self, GstCaps * caps)
{
  const GstStructure *str;
  const gchar *name;
  gint numerator, denominator;
  gboolean ret = TRUE;

  if (caps == NULL) {
    ret = FALSE;
  }

  str = gst_caps_get_structure (caps, 0);
  name = gst_structure_get_name (str);

  if (g_str_has_prefix (name, "video")) {
    GST_DEBUG_OBJECT (self, "Injector configured as VIDEO");
    self->priv->type = VIDEO;
    gst_structure_get_fraction (str, "framerate", &numerator, &denominator);

    //calculate waiting time based on caps
    KMS_BUFFER_INJECTOR_LOCK (self);
    if (numerator > 0) {
      self->priv->wait_time = ((GST_SECOND * denominator) / numerator) / 1000;
    } else {
      self->priv->wait_time = DEFAULT_WAITING_TIME;
    }
    KMS_BUFFER_INJECTOR_UNLOCK (self);

    GST_DEBUG ("Video: Wait time %" G_GINT64_FORMAT, self->priv->wait_time);
  } else {
    GST_DEBUG_OBJECT (self, "Injector configured as AUDIO");
    self->priv->type = AUDIO;
    ret = FALSE;
  }

  return ret;
}

static GstFlowReturn
kms_buffer_injector_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  KmsBufferInjector *buffer_injector = KMS_BUFFER_INJECTOR (parent);;

  KMS_BUFFER_INJECTOR_LOCK (buffer_injector);
  if ((buffer_injector->priv->type == AUDIO)
      && (!buffer_injector->priv->configured)) {
    //calculate waiting time based on buffer duration
    if ((GST_CLOCK_TIME_IS_VALID (buffer->duration)) && (buffer->duration > 0)) {
      buffer_injector->priv->wait_time = buffer->duration;
    } else {
      buffer_injector->priv->wait_time = DEFAULT_WAITING_TIME;
    }
    buffer_injector->priv->configured = TRUE;

    GST_DEBUG_OBJECT (buffer_injector, "Audio: Wait time %" G_GINT64_FORMAT,
        buffer_injector->priv->wait_time);
  }

  if (!buffer_injector->priv->configured) {
    gst_buffer_unref (buffer);
    KMS_BUFFER_INJECTOR_UNLOCK (buffer_injector);
    return GST_FLOW_OK;
  }

  gst_buffer_replace (&buffer_injector->priv->previous_buffer, buffer);

  KMS_BUFFER_INJECTOR_UNLOCK (buffer_injector);

  //wake up pad task
  g_mutex_lock (&buffer_injector->priv->mutex_generate);
  g_cond_signal (&buffer_injector->priv->cond_generate);
  g_mutex_unlock (&buffer_injector->priv->mutex_generate);

  return gst_pad_push (buffer_injector->priv->srcpad, buffer);
}

static gboolean
kms_buffer_injector_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  KmsBufferInjector *buffer_injector = KMS_BUFFER_INJECTOR (parent);

  buffer_injector = KMS_BUFFER_INJECTOR (parent);

  if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
    GstCaps *caps;

    gst_event_parse_caps (event, &caps);
    buffer_injector->priv->configured =
        kms_buffer_injector_config (buffer_injector, caps);
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
kms_buffer_injector_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;
  KmsBufferInjector *buffer_injector = KMS_BUFFER_INJECTOR (parent);

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      if (active) {
        res = gst_pad_start_task (pad,
            (GstTaskFunction) kms_buffer_injector_generate_buffers,
            buffer_injector, NULL);
      } else {
        res = gst_pad_stop_task (pad);
      }
      break;
    case GST_PAD_MODE_PULL:
      res = TRUE;
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
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
  GST_PAD_SET_PROXY_CAPS (self->priv->srcpad);

  gst_pad_set_activatemode_function (self->priv->srcpad,
      kms_buffer_injector_activate_mode);

  g_rec_mutex_init (&self->priv->thread_mutex);
  g_mutex_init (&self->priv->mutex_generate);
  g_cond_init (&self->priv->cond_generate);

  self->priv->wait_time = DEFAULT_WAITING_TIME;
  self->priv->configured = FALSE;
  self->priv->still_waiting = TRUE;
}

static GstStateChangeReturn
kms_buffer_injector_change_state (GstElement * element,
    GstStateChange transition)
{
  KmsBufferInjector *buffer_injector = KMS_BUFFER_INJECTOR (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&buffer_injector->priv->mutex_generate);
      buffer_injector->priv->still_waiting = FALSE;
      g_cond_signal (&buffer_injector->priv->cond_generate);
      g_mutex_unlock (&buffer_injector->priv->mutex_generate);
      gst_pad_pause_task (buffer_injector->priv->srcpad);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}

static void
kms_buffer_injector_finalize (GObject * object)
{
  KmsBufferInjector *buffer_injector = KMS_BUFFER_INJECTOR (object);

  g_rec_mutex_clear (&buffer_injector->priv->thread_mutex);
  g_mutex_clear (&buffer_injector->priv->mutex_generate);
  g_cond_clear (&buffer_injector->priv->cond_generate);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
kms_buffer_injector_class_init (KmsBufferInjectorClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_buffer_injector_finalize;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  gstelement_class->change_state = kms_buffer_injector_change_state;

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
  GST_DEBUG_REGISTER_FUNCPTR (kms_buffer_injector_handle_sink_event);
  GST_DEBUG_REGISTER_FUNCPTR (kms_buffer_injector_activate_mode);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  g_type_class_add_private (klass, sizeof (KmsBufferInjectorPrivate));
}

gboolean
kms_buffer_injector_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_BUFFER_INJECTOR);
}
