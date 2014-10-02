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

#define GST_DEFAULT_NAME "dectreebin"
#define GST_CAT_DEFAULT kms_dec_tree_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_dec_tree_bin_parent_class parent_class
G_DEFINE_TYPE (KmsDecTreeBin, kms_dec_tree_bin, KMS_TYPE_TREE_BIN);

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
