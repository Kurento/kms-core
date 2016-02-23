/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
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

#include <stdlib.h>

#include "sdp_utils.h"
#include "kmssdpagent.h"
#include "kmssdpulpfecext.h"
#include "kmsisdpmediaextension.h"
#include "kms-sdp-agent-marshal.h"

#define OBJECT_NAME "sdpulpfecext"

GST_DEBUG_CATEGORY_STATIC (kms_sdp_ulp_fec_ext_debug_category);
#define GST_CAT_DEFAULT kms_sdp_ulp_fec_ext_debug_category

#define parent_class kms_sdp_ulp_fec_ext_parent_class

static void kms_i_sdp_media_extension_init (KmsISdpMediaExtensionInterface *
    iface);

G_DEFINE_TYPE_WITH_CODE (KmsSdpUlpFecExt, kms_sdp_ulp_fec_ext,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_SDP_MEDIA_EXTENSION,
        kms_i_sdp_media_extension_init)
    GST_DEBUG_CATEGORY_INIT (kms_sdp_ulp_fec_ext_debug_category, OBJECT_NAME,
        0, "debug category for sdp ulp_fec_ext"));

#define ULP_FEC_RTPMAP "ulpfec"

enum
{
  SIGNAL_ON_OFFERED_ULP_FEC,

  LAST_SIGNAL
};

static guint obj_signals[LAST_SIGNAL] = { 0 };

static gboolean
kms_sdp_ulp_fec_ext_add_offer_attributes (KmsISdpMediaExtension * ext,
    GstSDPMedia * offer, GError ** error)
{
  /* So far, ulpfec is only supported on reception. */
  /* Do not add anything to the offer */
  return TRUE;
}

static gboolean
kms_sdp_ulp_fec_ext_is_pt_in_fmts (const GstSDPMedia * media, gint pt)
{
  guint i, len;

  len = gst_sdp_media_formats_len (media);

  for (i = 0; i < len; i++) {
    gint payload;

    payload = atoi (gst_sdp_media_get_format (media, i));

    if (payload == pt) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
kms_sdp_ulp_fec_ext_get_pt_and_clock_rate (KmsISdpMediaExtension * ext,
    const GstSDPMedia * media, gint * pt, gint * clock_rate)
{
  gboolean found = FALSE;
  guint8 i;

  for (i = 0;; i++) {
    const gchar *val = NULL;
    gchar **attrs;

    val = gst_sdp_media_get_attribute_val_n (media, "rtpmap", i);

    if (val == NULL) {
      break;
    }

    if (!g_str_match_string (ULP_FEC_RTPMAP, val, TRUE)) {
      continue;
    }

    attrs = g_strsplit (val, " ", 0);

    if (attrs[0] == NULL) {
      /* No pt */
      g_strfreev (attrs);
      continue;
    }

    if (attrs[1] == NULL) {
      /* No codec */
      g_strfreev (attrs);
      continue;
    }

    if (!kms_sdp_ulp_fec_ext_is_pt_in_fmts (media, atoi (attrs[0]))) {
      /* ulpfec pt is not in the offer */
      g_strfreev (attrs);
      continue;
    }

    if (!sdp_utils_get_data_from_rtpmap (attrs[1], NULL, clock_rate)) {
      g_strfreev (attrs);
      continue;
    }

    if (pt != NULL) {
      *pt = atoi (attrs[0]);
    }

    found = TRUE;

    g_strfreev (attrs);

    break;
  }

  return found;
}

static gboolean
kms_sdp_ulp_fec_ext_add_ulp_fec_pt (KmsISdpMediaExtension * ext,
    GstSDPMedia * media, gint pt, gint clock_rate, GError ** error)
{
  gboolean ret;
  gchar *fmt;
  guint len;

  len = gst_sdp_media_formats_len (media);

  if (len == 0) {
    GST_WARNING_OBJECT (ext, "No medias to protect");
    return TRUE;
  }

  if (kms_sdp_ulp_fec_ext_is_pt_in_fmts (media, pt)) {
    g_set_error (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER,
        "Payload type (%i) already assigned in the SDP media", pt);
    return FALSE;
  }

  fmt = g_strdup_printf ("%i", pt);

  if (sdp_utils_get_attr_map_value (media, "rtpmap", fmt) != NULL) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_UNEXPECTED_ERROR,
        "rtpmap attribute for ulpfec already added");
    g_free (fmt);
    return FALSE;
  }

  ret = gst_sdp_media_add_format (media, fmt) == GST_SDP_OK;
  g_free (fmt);

  if (!ret) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_UNEXPECTED_ERROR, "Can not add ulpfec payload");
    return FALSE;
  }

  return ret;
}

static gboolean
kms_sdp_ulp_fec_ext_add_answer_attributes (KmsISdpMediaExtension * ext,
    const GstSDPMedia * offer, GstSDPMedia * answer, GError ** error)
{
  gboolean managed = FALSE;
  gint pt, clock_rate;

  if (!kms_sdp_ulp_fec_ext_get_pt_and_clock_rate (ext, offer, &pt, &clock_rate)) {
    return TRUE;
  }

  g_signal_emit (G_OBJECT (ext), obj_signals[SIGNAL_ON_OFFERED_ULP_FEC], 0, pt,
      clock_rate, &managed);

  if (!managed) {
    /* Do not add atributes to the answer */
    return TRUE;
  }

  return kms_sdp_ulp_fec_ext_add_ulp_fec_pt (ext, answer, pt, clock_rate,
      error);
}

static gboolean
kms_sdp_ulp_fec_ext_can_insert_attribute (KmsISdpMediaExtension * ext,
    const GstSDPMedia * offer, const GstSDPAttribute * attr,
    GstSDPMedia * answer, SdpMessageContext * ctx)
{
  return FALSE;
}

static gboolean
kms_sdp_ulp_fec_ext_process_answer_attributes (KmsISdpMediaExtension * ext,
    const GstSDPMedia * answer, GError ** error)
{
  /* So far, ulpfec is only supported on reception. */
  /* This callback is only called as response to a previous offer */
  return TRUE;
}

static void
kms_sdp_ulp_fec_ext_class_init (KmsSdpUlpFecExtClass * klass)
{
  obj_signals[SIGNAL_ON_OFFERED_ULP_FEC] =
      g_signal_new ("on-offered-ulp-fec",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsSdpUlpFecExtClass, on_offered_ulpfec),
      g_signal_accumulator_true_handled, NULL,
      __kms_sdp_agent_marshal_BOOLEAN__UINT_UINT, G_TYPE_BOOLEAN, 2,
      G_TYPE_UINT, G_TYPE_UINT);
}

static void
kms_sdp_ulp_fec_ext_init (KmsSdpUlpFecExt * self)
{
  /* Nothing to do */
}

static void
kms_i_sdp_media_extension_init (KmsISdpMediaExtensionInterface * iface)
{
  iface->add_offer_attributes = kms_sdp_ulp_fec_ext_add_offer_attributes;
  iface->add_answer_attributes = kms_sdp_ulp_fec_ext_add_answer_attributes;
  iface->can_insert_attribute = kms_sdp_ulp_fec_ext_can_insert_attribute;
  iface->process_answer_attributes =
      kms_sdp_ulp_fec_ext_process_answer_attributes;
}

KmsSdpUlpFecExt *
kms_sdp_ulp_fec_ext_new ()
{
  gpointer obj;

  obj = g_object_new (KMS_TYPE_SDP_ULP_FEC_EXT, NULL);

  return KMS_SDP_ULP_FEC_EXT (obj);
}
