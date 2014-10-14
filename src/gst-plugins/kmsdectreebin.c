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

#include "kmsdectreebin.h"
#include "kmsutils.h"

#define GST_DEFAULT_NAME "dectreebin"
#define GST_CAT_DEFAULT kms_dec_tree_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_dec_tree_bin_parent_class parent_class
G_DEFINE_TYPE (KmsDecTreeBin, kms_dec_tree_bin, KMS_TYPE_TREE_BIN);

static GstElement *
create_decoder_for_caps (const GstCaps * caps, const GstCaps * raw_caps)
{
  GList *decoder_list, *filtered_list, *aux_list, *l;
  GstElementFactory *decoder_factory = NULL;
  GstElement *decoder = NULL;

  decoder_list =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_DECODER,
      GST_RANK_NONE);
  aux_list =
      gst_element_factory_list_filter (decoder_list, caps, GST_PAD_SINK, FALSE);
  filtered_list =
      gst_element_factory_list_filter (aux_list, raw_caps, GST_PAD_SRC, FALSE);

  for (l = filtered_list; l != NULL && decoder_factory == NULL; l = l->next) {
    decoder_factory = GST_ELEMENT_FACTORY (l->data);
    if (gst_element_factory_get_num_pad_templates (decoder_factory) != 2)
      decoder_factory = NULL;
  }

  if (decoder_factory != NULL) {
    decoder = gst_element_factory_create (decoder_factory, NULL);
  }

  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (decoder_list);
  gst_plugin_feature_list_free (aux_list);

  return decoder;
}

static gboolean
kms_dec_tree_bin_configure (KmsDecTreeBin * self, const GstCaps * caps,
    const GstCaps * raw_caps)
{
  KmsTreeBin *tree_bin = KMS_TREE_BIN (self);
  GstElement *input_queue, *dec, *output_tee;
  GstPad *pad;

  dec = create_decoder_for_caps (caps, raw_caps);
  if (dec == NULL) {
    GST_WARNING_OBJECT (self, "Invalid decoder for caps %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }
  GST_DEBUG_OBJECT (self, "Decoder found: %" GST_PTR_FORMAT, dec);

  pad = gst_element_get_static_pad (dec, "sink");
  kms_utils_drop_until_keyframe (pad, TRUE);
  gst_object_unref (pad);

  gst_bin_add (GST_BIN (self), dec);
  gst_element_sync_state_with_parent (dec);

  input_queue = kms_tree_bin_get_input_queue (tree_bin);
  output_tee = kms_tree_bin_get_output_tee (tree_bin);
  gst_element_link_many (input_queue, dec, output_tee, NULL);

  return TRUE;
}

KmsDecTreeBin *
kms_dec_tree_bin_new (const GstCaps * caps, const GstCaps * raw_caps)
{
  GObject *dec;

  dec = g_object_new (KMS_TYPE_DEC_TREE_BIN, NULL);
  if (!kms_dec_tree_bin_configure (KMS_DEC_TREE_BIN (dec), caps, raw_caps)) {
    g_object_unref (dec);
    return NULL;
  }

  return KMS_DEC_TREE_BIN (dec);
}

static void
kms_dec_tree_bin_init (KmsDecTreeBin * self)
{
  /* Nothing to do */
}

static void
kms_dec_tree_bin_class_init (KmsDecTreeBinClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "DecTreeBin",
      "Generic",
      "Bin to decode and distribute RAW media.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}
