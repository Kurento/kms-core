/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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
#include "kmsdummyuri.h"

#define PLUGIN_NAME "dummyuri"

GST_DEBUG_CATEGORY_STATIC (kms_dummy_uri_debug_category);
#define GST_CAT_DEFAULT kms_dummy_uri_debug_category

#define kms_dummy_uri_parent_class parent_class

#define KMS_DUMMY_URI_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (             \
    (obj),                                  \
    KMS_TYPE_DUMMY_URI,                  \
    KmsDummyUriPrivate                   \
  )                                         \
)

G_DEFINE_TYPE_WITH_CODE (KmsDummyUri, kms_dummy_uri,
    KMS_TYPE_URI_ENDPOINT,
    GST_DEBUG_CATEGORY_INIT (kms_dummy_uri_debug_category, PLUGIN_NAME,
        0, "debug category for kurento dummy plugin"));

/* Object properties */
enum
{
  PROP_0,
  N_PROPERTIES
};

static void
kms_dummy_uri_class_init (KmsDummyUriClass * klass)
{
  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
      "KmsDummyUri",
      "Generic",
      "Dummy uri element", "David Fernandez <d.fernandezlop@gmail.com>");
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);
}

static void
kms_dummy_uri_init (KmsDummyUri * self)
{
}

gboolean
kms_dummy_uri_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_DUMMY_URI);
}
