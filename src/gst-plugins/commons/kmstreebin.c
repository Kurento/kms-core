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

#include "kmstreebin.h"

#define GST_DEFAULT_NAME "treebin"
#define GST_CAT_DEFAULT kms_tree_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_tree_bin_parent_class parent_class
G_DEFINE_TYPE (KmsTreeBin, kms_tree_bin, GST_TYPE_BIN);

#define KMS_TREE_BIN_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (         \
    (obj),                              \
    KMS_TYPE_TREE_BIN,                  \
    KmsTreeBinPrivate                   \
  )                                     \
)

struct _KmsTreeBinPrivate
{
  GstElement *input_element, *output_tee;
  GstCaps *input_caps;
};

GstElement *
kms_tree_bin_get_input_element (KmsTreeBin * self)
{
  return self->priv->input_element;
}

void
kms_tree_bin_set_input_element (KmsTreeBin * self, GstElement * input_element)
{
  self->priv->input_element = input_element;
}

GstElement *
kms_tree_bin_get_output_tee (KmsTreeBin * self)
{
  return self->priv->output_tee;
}

void
kms_tree_bin_unlink_input_element_from_tee (KmsTreeBin * self)
{
  GstPad *queue_sink, *peer, *tee_src;
  GstElement *tee;

  queue_sink = gst_element_get_static_pad (self->priv->input_element, "sink");
  peer = gst_pad_get_peer (queue_sink);

  if (GST_IS_PROXY_PAD (peer)) {
    GstProxyPad *ghost;

    ghost = gst_proxy_pad_get_internal (GST_PROXY_PAD (peer));
    tee_src = gst_pad_get_peer (GST_PAD (ghost));

    g_object_unref (peer);
    g_object_unref (ghost);
  } else {
    tee_src = peer;
  }

  gst_pad_unlink (tee_src, queue_sink);

  tee = gst_pad_get_parent_element (tee_src);
  if (tee != NULL) {
    gst_element_release_request_pad (tee, tee_src);
    g_object_unref (tee);
  }

  g_object_unref (tee_src);
  g_object_unref (queue_sink);
}

GstCaps *
kms_tree_bin_get_input_caps (KmsTreeBin * self)
{
  return self->priv->input_caps;
}

static void
kms_tree_bin_set_input_caps (KmsTreeBin * self, GstCaps * caps)
{
  if (self->priv->input_caps) {
    gst_caps_unref (self->priv->input_caps);
    self->priv->input_caps = NULL;
  }

  self->priv->input_caps = gst_caps_ref (caps);
}

static gboolean
tee_query_function (GstPad * pad, GstObject * parent, GstQuery * query)
{
  if (GST_QUERY_TYPE (query) == GST_QUERY_ACCEPT_CAPS) {
    GstCaps *caps;
    KmsTreeBin *self = KMS_TREE_BIN (GST_OBJECT_PARENT (parent));

    gst_query_parse_accept_caps (query, &caps);

    kms_tree_bin_set_input_caps (self, caps);
  }

  return gst_pad_query_default (pad, parent, query);
}

static gboolean
tee_event_function (GstPad * pad, GstObject * parent, GstEvent * event)
{
  if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
    GstCaps *caps;
    KmsTreeBin *self = KMS_TREE_BIN (GST_OBJECT_PARENT (parent));

    gst_event_parse_caps (event, &caps);

    kms_tree_bin_set_input_caps (self, caps);
  }

  return gst_pad_event_default (pad, parent, event);
}

static void
kms_tree_bin_finalize (GObject * object)
{
  KmsTreeBin *self = KMS_TREE_BIN_CAST (object);

  if (self->priv->input_caps) {
    gst_caps_unref (self->priv->input_caps);
    self->priv->input_caps = NULL;
  }
  /* chain up */
  G_OBJECT_CLASS (kms_tree_bin_parent_class)->finalize (object);
}

static void
kms_tree_bin_init (KmsTreeBin * self)
{
  GstElement *fakesink;
  GstPad *sink;

  self->priv = KMS_TREE_BIN_GET_PRIVATE (self);

  self->priv->output_tee = gst_element_factory_make ("tee", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (fakesink, "async", FALSE, "sync", FALSE, NULL);

  sink = gst_element_get_static_pad (self->priv->output_tee, "sink");
  if (sink) {
    gst_pad_set_query_function (sink, tee_query_function);
    gst_pad_set_event_function (sink, tee_event_function);
    g_object_unref (sink);
  }

  gst_bin_add_many (GST_BIN (self), self->priv->output_tee, fakesink, NULL);
  gst_element_link (self->priv->output_tee, fakesink);
}

static void
kms_tree_bin_class_init (KmsTreeBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TreeBin",
      "Generic",
      "Base bin to manage elements for media distribution.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gobject_class->finalize = kms_tree_bin_finalize;

  g_type_class_add_private (klass, sizeof (KmsTreeBinPrivate));
}
