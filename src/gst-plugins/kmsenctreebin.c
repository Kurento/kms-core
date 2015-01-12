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

#include "kmsenctreebin.h"
#include "kmsutils.h"

#define GST_DEFAULT_NAME "enctreebin"
#define GST_CAT_DEFAULT kms_enc_tree_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_enc_tree_bin_parent_class parent_class
G_DEFINE_TYPE (KmsEncTreeBin, kms_enc_tree_bin, KMS_TYPE_TREE_BIN);

#define KMS_ENC_TREE_BIN_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (             \
    (obj),                                  \
    KMS_TYPE_ENC_TREE_BIN,                  \
    KmsEncTreeBinPrivate                    \
  )                                         \
)

struct _KmsEncTreeBinPrivate
{
  GstPad *enc_sink;
  gulong remb_manager_probe_id;
  RembEventManager *remb_manager;
};

static void
configure_encoder (GstElement * encoder, const gchar * factory_name)
{
  GST_DEBUG ("Configure encoder: %s", factory_name);
  if (g_strcmp0 ("vp8enc", factory_name) == 0) {
    g_object_set (G_OBJECT (encoder), "deadline", G_GINT64_CONSTANT (200000),
        "threads", 1, "cpu-used", 16, "resize-allowed", TRUE,
        "target-bitrate", 300000, "end-usage", /* cbr */ 1, NULL);
  } else if (g_strcmp0 ("x264enc", factory_name) == 0) {
    g_object_set (G_OBJECT (encoder), "speed-preset", 1 /* ultrafast */ ,
        "tune", 4 /* zerolatency */ , "threads", (guint) 1,
        NULL);
  }
}

static GstElement *
create_encoder_for_caps (const GstCaps * caps)
{
  GList *encoder_list, *filtered_list, *l;
  GstElementFactory *encoder_factory = NULL;
  GstElement *encoder = NULL;

  encoder_list =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER,
      GST_RANK_NONE);
  filtered_list =
      gst_element_factory_list_filter (encoder_list, caps, GST_PAD_SRC, FALSE);

  for (l = filtered_list; l != NULL && encoder_factory == NULL; l = l->next) {
    encoder_factory = GST_ELEMENT_FACTORY (l->data);
    if (gst_element_factory_get_num_pad_templates (encoder_factory) != 2)
      encoder_factory = NULL;
  }

  if (encoder_factory != NULL) {
    encoder = gst_element_factory_create (encoder_factory, NULL);
    configure_encoder (encoder, GST_OBJECT_NAME (encoder_factory));
  }

  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (encoder_list);

  return encoder;
}

static void
enc_set_target_bitrate (GstElement * enc, gint target_bitrate)
{
  gchar *name;

  g_object_get (enc, "name", &name, NULL);

  if (g_str_has_prefix (name, "vp8enc")) {
    gint last_br;

    g_object_get (enc, "target-bitrate", &last_br, NULL);
    if (last_br / 1000 != target_bitrate / 1000) {
      GST_DEBUG_OBJECT (enc, "Set bitrate: %" G_GUINT32_FORMAT, target_bitrate);
      g_object_set (enc, "target-bitrate", target_bitrate, NULL);
    }
  } else if (g_str_has_prefix (name, "x264enc")) {
    gint last_br, new_br = target_bitrate / 1000;

    g_object_get (enc, "bitrate", &last_br, NULL);
    if (last_br != new_br) {
      GST_DEBUG_OBJECT (enc, "Set bitrate: %" G_GUINT32_FORMAT, target_bitrate);
      g_object_set (enc, "target-bitrate", new_br, NULL);
    }
  }

  g_free (name);
}

static GstPadProbeReturn
config_enc_bitrate_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  RembEventManager *remb_manager = user_data;
  GstElement *enc;
  guint br = kms_utils_remb_event_manager_get_min (remb_manager);

  if (br == 0) {
    return GST_PAD_PROBE_OK;
  }

  enc = gst_pad_get_parent_element (pad);
  enc_set_target_bitrate (enc, br);
  g_object_unref (enc);

  return GST_PAD_PROBE_OK;
}

static gboolean
kms_enc_tree_bin_configure (KmsEncTreeBin * self, const GstCaps * caps)
{
  KmsTreeBin *tree_bin = KMS_TREE_BIN (self);
  GstElement *rate, *convert, *mediator, *enc, *output_tee;

  enc = create_encoder_for_caps (caps);
  if (enc == NULL) {
    GST_WARNING_OBJECT (self, "Invalid encoder for caps: %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }
  GST_DEBUG_OBJECT (self, "Encoder found: %" GST_PTR_FORMAT, enc);

  self->priv->enc_sink = gst_element_get_static_pad (enc, "sink");
  self->priv->remb_manager =
      kms_utils_remb_event_manager_create (self->priv->enc_sink);
  self->priv->remb_manager_probe_id =
      gst_pad_add_probe (self->priv->enc_sink, GST_PAD_PROBE_TYPE_BUFFER,
      config_enc_bitrate_probe, self->priv->remb_manager, NULL);

  rate = kms_utils_create_rate_for_caps (caps);
  convert = kms_utils_create_convert_for_caps (caps);
  mediator = kms_utils_create_mediator_element (caps);

  gst_bin_add_many (GST_BIN (self), rate, convert, mediator, enc, NULL);
  gst_element_sync_state_with_parent (enc);
  gst_element_sync_state_with_parent (mediator);
  gst_element_sync_state_with_parent (convert);
  gst_element_sync_state_with_parent (rate);

  kms_tree_bin_set_input_element (tree_bin, rate);
  output_tee = kms_tree_bin_get_output_tee (tree_bin);
  gst_element_link_many (rate, convert, mediator, enc, output_tee, NULL);

  return TRUE;
}

KmsEncTreeBin *
kms_enc_tree_bin_new (const GstCaps * caps)
{
  GObject *enc;

  enc = g_object_new (KMS_TYPE_ENC_TREE_BIN, NULL);
  if (!kms_enc_tree_bin_configure (KMS_ENC_TREE_BIN (enc), caps)) {
    g_object_unref (enc);
    return NULL;
  }

  return KMS_ENC_TREE_BIN (enc);
}

static void
kms_enc_tree_bin_init (KmsEncTreeBin * self)
{
  self->priv = KMS_ENC_TREE_BIN_GET_PRIVATE (self);

  self->priv->enc_sink = NULL;
  self->priv->remb_manager = NULL;
  self->priv->remb_manager_probe_id = 0L;
}

static void
kms_enc_tree_bin_dispose (GObject * object)
{
  KmsEncTreeBin *self = KMS_ENC_TREE_BIN (object);

  GST_DEBUG_OBJECT (object, "dispose");
  if (self->priv->enc_sink) {
    if (self->priv->remb_manager_probe_id) {
      gst_pad_remove_probe (self->priv->enc_sink,
          self->priv->remb_manager_probe_id);

      self->priv->remb_manager_probe_id = 0L;
    }

    g_clear_object (&self->priv->enc_sink);
  }

  if (self->priv->remb_manager) {
    kms_utils_remb_event_manager_destroy (self->priv->remb_manager);
    self->priv->remb_manager = NULL;
  }

  /* chain up */
  G_OBJECT_CLASS (kms_enc_tree_bin_parent_class)->dispose (object);
}

static void
kms_enc_tree_bin_class_init (KmsEncTreeBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "EncTreeBin",
      "Generic",
      "Bin to encode and distribute encoding media.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gobject_class->dispose = kms_enc_tree_bin_dispose;

  g_type_class_add_private (klass, sizeof (KmsEncTreeBinPrivate));
}
