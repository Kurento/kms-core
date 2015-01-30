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
#include "kmselement.h"
#include "kmspassthrough.h"
#include "kmsagnosticcaps.h"
#include "kmsutils.h"
#include "kms-core-enumtypes.h"
#include "kmsfiltertype.h"

#define PLUGIN_NAME "passthrough"

#define DEFAULT_FILTER_TYPE KMS_FILTER_TYPE_AUTODETECT

GST_DEBUG_CATEGORY_STATIC (kms_pass_through_debug_category);
#define GST_CAT_DEFAULT kms_pass_through_debug_category

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsPassThrough, kms_pass_through,
    KMS_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (kms_pass_through_debug_category, PLUGIN_NAME,
        0, "debug category for passthrough element"));

static void
kms_pass_through_connect_passthrough (KmsPassThrough * self,
    KmsElementPadType type, GstElement * agnosticbin)
{
  GstPad *target = gst_element_get_static_pad (agnosticbin, "sink");

  kms_element_connect_sink_target (KMS_ELEMENT (self), target, type);
  g_object_unref (target);
}

static void
kms_pass_through_class_init (KmsPassThroughClass * klass)
{
  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "PassThrough", "Generic/KmsElement", "Kurento pass_through",
      "Jose Antonio Santos Cadenas <santoscadenas@gmail.com>");
}

static void
kms_pass_through_init (KmsPassThrough * self)
{
  kms_pass_through_connect_passthrough (self, KMS_ELEMENT_PAD_TYPE_AUDIO,
      kms_element_get_audio_agnosticbin (KMS_ELEMENT (self)));
  kms_pass_through_connect_passthrough (self, KMS_ELEMENT_PAD_TYPE_VIDEO,
      kms_element_get_video_agnosticbin (KMS_ELEMENT (self)));
}

gboolean
kms_pass_through_plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_PASS_THROUGH);
}
