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

#include "kmsparsetreebin.h"

#define GST_DEFAULT_NAME "parsetreebin"
#define GST_CAT_DEFAULT kms_parse_tree_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_parse_tree_bin_parent_class parent_class
G_DEFINE_TYPE (KmsParseTreeBin, kms_parse_tree_bin, KMS_TYPE_TREE_BIN);

#define KMS_PARSE_TREE_BIN_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (         \
    (obj),                              \
    KMS_TYPE_PARSE_TREE_BIN,                  \
    KmsParseTreeBinPrivate                   \
  )                                     \
)

struct _KmsParseTreeBinPrivate
{
  GstElement *parser;
};

static GstElement *
create_parser_for_caps (const GstCaps * caps)
{
  GList *parser_list, *filtered_list, *l;
  GstElementFactory *parser_factory = NULL;
  GstElement *parser = NULL;

  parser_list =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_PARSER |
      GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_NONE);
  filtered_list =
      gst_element_factory_list_filter (parser_list, caps, GST_PAD_SINK, FALSE);

  for (l = filtered_list; l != NULL && parser_factory == NULL; l = l->next) {
    parser_factory = GST_ELEMENT_FACTORY (l->data);
    if (gst_element_factory_get_num_pad_templates (parser_factory) != 2)
      parser_factory = NULL;
  }

  if (parser_factory != NULL) {
    parser = gst_element_factory_create (parser_factory, NULL);
  } else {
    parser = gst_element_factory_make ("capsfilter", NULL);
  }

  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (parser_list);

  return parser;
}

static void
kms_parse_tree_bin_configure (KmsParseTreeBin * self, const GstCaps * caps)
{
  KmsTreeBin *tree_bin = KMS_TREE_BIN (self);
  GstElement *input_queue, *output_tee;

  self->priv->parser = create_parser_for_caps (caps);

  gst_bin_add (GST_BIN (self), self->priv->parser);
  gst_element_sync_state_with_parent (self->priv->parser);

  input_queue = kms_tree_bin_get_input_queue (tree_bin);
  output_tee = kms_tree_bin_get_output_tee (tree_bin);
  gst_element_link_many (input_queue, self->priv->parser, output_tee, NULL);
}

KmsParseTreeBin *
kms_parse_tree_bin_new (const GstCaps * caps)
{
  GObject *parse;

  parse = g_object_new (KMS_TYPE_PARSE_TREE_BIN, NULL);
  kms_parse_tree_bin_configure (KMS_PARSE_TREE_BIN (parse), caps);

  return KMS_PARSE_TREE_BIN (parse);
}

GstElement *
kms_parse_tree_bin_get_parser (KmsParseTreeBin * self)
{
  return self->priv->parser;
}

static void
kms_parse_tree_bin_init (KmsParseTreeBin * self)
{
  self->priv = KMS_PARSE_TREE_BIN_GET_PRIVATE (self);
}

static void
kms_parse_tree_bin_class_init (KmsParseTreeBinClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "ParseTreeBin",
      "Generic",
      "Bin to parse and distribute media.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  g_type_class_add_private (klass, sizeof (KmsParseTreeBinPrivate));
}
