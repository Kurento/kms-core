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

#define KMS_ENC_TREE_BIN_LIMIT(obj, value) \
  MAX((obj)->priv->min_bitrate,MIN((obj)->priv->max_bitrate, (value)))

typedef enum
{
  VP8,
  X264,
  OPENH264,
  OPUS,
  UNSUPPORTED
} EncoderType;

struct _KmsEncTreeBinPrivate
{
  GstPad *enc_sink;
  GstElement *enc;
  EncoderType enc_type;
  RembEventManager *remb_manager;

  gint remb_bitrate;
  gint tag_bitrate;

  gint current_bitrate;

  gint max_bitrate;
  gint min_bitrate;
};

static void
configure_encoder (GstElement * encoder, EncoderType type, gint target_bitrate)
{
  GST_DEBUG ("Configure encoder: %" GST_PTR_FORMAT, encoder);
  switch (type) {
    case VP8:
    {
      /* *INDENT-OFF* */
      g_object_set (G_OBJECT (encoder),
                    "deadline", G_GINT64_CONSTANT (200000),
                    "threads", 1,
                    "cpu-used", 16,
                    "resize-allowed", TRUE,
                    "target-bitrate", target_bitrate,
                    "end-usage", /* cbr */ 1,
                    NULL);
      /* *INDENT-ON* */
      break;
    }
    case X264:
    {
      /* *INDENT-OFF* */
      g_object_set (G_OBJECT (encoder),
                    "speed-preset", /* veryfast */ 3,
                    "threads", (guint) 1,
                    "bitrate", target_bitrate / 1000,
                    "key-int-max", 60,
                    "tune", /* zero-latency */ 4,
                    NULL);
      /* *INDENT-ON* */
      break;
    }
    case OPENH264:
    {
      /* *INDENT-OFF* */
      g_object_set (G_OBJECT (encoder),
                    "rate-control", /* bitrate */ 1,
                    "bitrate", target_bitrate,
                    NULL);
      /* *INDENT-ON* */
      break;
    }
    case OPUS:
    {
      g_object_set (G_OBJECT (encoder), "inband-fec", TRUE, NULL);
      break;
    }
    default:
      GST_DEBUG ("Codec %" GST_PTR_FORMAT
          " not configured because it is not supported", encoder);
      break;
  }
}

static void
kms_enc_tree_bin_set_encoder_type (KmsEncTreeBin * self)
{
  gchar *name;

  g_object_get (self->priv->enc, "name", &name, NULL);

  if (g_str_has_prefix (name, "vp8enc")) {
    self->priv->enc_type = VP8;
  } else if (g_str_has_prefix (name, "x264enc")) {
    self->priv->enc_type = X264;
  } else if (g_str_has_prefix (name, "openh264enc")) {
    self->priv->enc_type = OPENH264;
  } else if (g_str_has_prefix (name, "opusenc")) {
    self->priv->enc_type = OPUS;
  } else {
    self->priv->enc_type = UNSUPPORTED;
  }

  g_free (name);
}

static void
kms_enc_tree_bin_create_encoder_for_caps (KmsEncTreeBin * self,
    const GstCaps * caps, gint target_bitrate)
{
  GList *encoder_list, *filtered_list, *l;
  GstElementFactory *encoder_factory = NULL;

  encoder_list =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER,
      GST_RANK_NONE);

  /* HACK: Augment the openh264 rank */
  for (l = encoder_list; l != NULL; l = l->next) {
    encoder_factory = GST_ELEMENT_FACTORY (l->data);

    if (g_str_has_prefix (GST_OBJECT_NAME (encoder_factory), "openh264")) {
      encoder_list = g_list_remove (encoder_list, l->data);
      encoder_list = g_list_prepend (encoder_list, encoder_factory);
      break;
    }
  }

  encoder_factory = NULL;
  filtered_list =
      gst_element_factory_list_filter (encoder_list, caps, GST_PAD_SRC, FALSE);

  for (l = filtered_list; l != NULL && encoder_factory == NULL; l = l->next) {
    encoder_factory = GST_ELEMENT_FACTORY (l->data);
    if (gst_element_factory_get_num_pad_templates (encoder_factory) != 2)
      encoder_factory = NULL;
  }

  if (encoder_factory != NULL) {
    self->priv->enc = gst_element_factory_create (encoder_factory, NULL);
    kms_enc_tree_bin_set_encoder_type (self);
    configure_encoder (self->priv->enc, self->priv->enc_type, target_bitrate);
  }

  gst_plugin_feature_list_free (filtered_list);
  gst_plugin_feature_list_free (encoder_list);
}

static gint
kms_enc_tree_bin_get_bitrate (KmsEncTreeBin * self)
{
  gint bitrate;

  if (self->priv->remb_bitrate <= 0) {
    bitrate = KMS_ENC_TREE_BIN_LIMIT (self, self->priv->tag_bitrate);
  } else if (self->priv->tag_bitrate <= 0) {
    bitrate = KMS_ENC_TREE_BIN_LIMIT (self, self->priv->remb_bitrate);
  } else {
    bitrate = KMS_ENC_TREE_BIN_LIMIT (self, MIN (self->priv->remb_bitrate,
            self->priv->tag_bitrate));
  }

  if (bitrate <= 0) {
    bitrate = self->priv->current_bitrate;
  } else {
    self->priv->current_bitrate = bitrate;
  }

  return bitrate;
}

static void
kms_enc_tree_bin_set_target_bitrate (KmsEncTreeBin * self)
{
  gint target_bitrate = kms_enc_tree_bin_get_bitrate (self);

  if (target_bitrate <= 0) {
    return;
  }

  GST_DEBUG_OBJECT (self->priv->enc, "Setting encoding bitrate to: %d",
      target_bitrate);

  switch (self->priv->enc_type) {
    case VP8:
    {
      gint last_br;

      g_object_get (self->priv->enc, "target-bitrate", &last_br, NULL);
      if (last_br / 1000 != target_bitrate / 1000) {
        GST_DEBUG_OBJECT (self->priv->enc, "Set bitrate: %" G_GUINT32_FORMAT,
            target_bitrate);
        g_object_set (self->priv->enc, "target-bitrate", target_bitrate, NULL);
      }
      break;
    }
    case X264:
    {
      guint last_br, new_br = target_bitrate / 1000;

      g_object_get (self->priv->enc, "bitrate", &last_br, NULL);
      if (last_br != new_br) {
        GST_DEBUG_OBJECT (self->priv->enc, "Set bitrate: %" G_GUINT32_FORMAT,
            target_bitrate);
        g_object_set (self->priv->enc, "bitrate", new_br, NULL);
      }
      break;
    }
    case OPENH264:
    {
      guint last_br, new_br = target_bitrate;

      g_object_get (self->priv->enc, "bitrate", &last_br, NULL);
      if (last_br / 1000 != new_br / 1000) {
        GST_DEBUG_OBJECT (self->priv->enc, "Set bitrate: %" G_GUINT32_FORMAT,
            target_bitrate);
        g_object_set (self->priv->enc, "bitrate", new_br, NULL);
      }
    }
    default:
      GST_DEBUG ("Not setting bitrate, encoder not supported");
      break;
  }
}

void
kms_enc_tree_bin_set_bitrate_limits (KmsEncTreeBin * self, gint min_bitrate,
    gint max_bitrate)
{
  // TODO: Think about adding a mutex here
  self->priv->max_bitrate = max_bitrate;
  self->priv->min_bitrate = min_bitrate;

  kms_enc_tree_bin_set_target_bitrate (self);
}

static void
bitrate_callback (RembEventManager * remb_manager, guint bitrate,
    gpointer user_data)
{
  KmsEncTreeBin *self = user_data;

  if (bitrate != 0) {
    self->priv->remb_bitrate = bitrate;
    kms_enc_tree_bin_set_target_bitrate (self);
  }
}

static GstPadProbeReturn
tag_event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *event = gst_pad_probe_info_get_event (info);

  if (GST_EVENT_TYPE (event) == GST_EVENT_TAG) {
    KmsEncTreeBin *self = data;
    GstTagList *taglist;
    guint bitrate;

    gst_event_parse_tag (event, &taglist);
    if (gst_tag_list_get_uint (taglist, "bitrate", &bitrate)) {

      self->priv->tag_bitrate = bitrate;
      kms_enc_tree_bin_set_target_bitrate (self);
    }
  }

  return GST_PAD_PROBE_OK;
}

/*
 * FIXME: This is a hack to make x264 work.
 *
 * We have notice that x264 doesn't work if width or height is odd,
 * so we force a rescale increasing one pixel that dimension
 * when we detect this situation.
 */
static GstPadProbeReturn
check_caps_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  int width, height;
  GstCaps *filter_caps, *caps;
  GstElement *element;
  GstStructure *st;
  gboolean needs_filter = FALSE;
  GstEvent *event = gst_pad_probe_info_get_event (info);

  if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
    return GST_PAD_PROBE_OK;
  }

  gst_event_parse_caps (event, &caps);

  st = gst_caps_get_structure (caps, 0);

  gst_structure_get (st, "width", G_TYPE_INT, &width, NULL);
  gst_structure_get (st, "height", G_TYPE_INT, &height, NULL);

  if (width % 2) {
    GST_WARNING ("Width is odd");
    needs_filter = TRUE;
    width--;
  }

  if (height % 2) {
    GST_WARNING ("Height is odd");
    needs_filter = TRUE;
    height--;
  }

  if (!needs_filter)
    return GST_PAD_PROBE_OK;

  filter_caps = gst_caps_from_string ("video/x-raw,format=I420");

  gst_caps_set_simple (filter_caps, "width", G_TYPE_INT, width, NULL);
  gst_caps_set_simple (filter_caps, "height", G_TYPE_INT, height, NULL);

  element = gst_pad_get_parent_element (pad);
  g_object_set (element, "caps", filter_caps, NULL);
  gst_caps_unref (filter_caps);
  g_object_unref (element);

  return GST_PAD_PROBE_OK;
}

static gboolean
kms_enc_tree_bin_configure (KmsEncTreeBin * self, const GstCaps * caps,
    gint target_bitrate)
{
  KmsTreeBin *tree_bin = KMS_TREE_BIN (self);
  GstElement *rate, *convert, *mediator, *output_tee, *capsfilter = NULL;

  self->priv->current_bitrate = target_bitrate;

  kms_enc_tree_bin_create_encoder_for_caps (self, caps, target_bitrate);

  if (self->priv->enc == NULL) {
    GST_WARNING_OBJECT (self, "Invalid encoder for caps: %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Encoder found: %" GST_PTR_FORMAT, self->priv->enc);

  self->priv->enc_sink = gst_element_get_static_pad (self->priv->enc, "sink");
  self->priv->remb_manager =
      kms_utils_remb_event_manager_create (self->priv->enc_sink);
  kms_utils_remb_event_manager_set_callback (self->priv->remb_manager,
      bitrate_callback, self, NULL);
  gst_pad_add_probe (self->priv->enc_sink, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      tag_event_probe, self, NULL);

  rate = kms_utils_create_rate_for_caps (caps);
  convert = kms_utils_create_convert_for_caps (caps);
  mediator = kms_utils_create_mediator_element (caps);

  gst_bin_add_many (GST_BIN (self), rate, convert, mediator, self->priv->enc,
      NULL);
  gst_element_sync_state_with_parent (self->priv->enc);
  gst_element_sync_state_with_parent (mediator);
  gst_element_sync_state_with_parent (convert);
  gst_element_sync_state_with_parent (rate);
  // FIXME: This is a hack to avoid an error on x264enc that does not work
  // properly with some raw formats, this should be fixed in gstreamer
  // but until this is done this hack makes it work
  if (self->priv->enc_type == X264) {
    GstCaps *filter_caps = gst_caps_from_string ("video/x-raw,format=I420");
    GstPad *sink;

    capsfilter = gst_element_factory_make ("capsfilter", NULL);
    sink = gst_element_get_static_pad (capsfilter, "sink");
    gst_pad_add_probe (sink, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        check_caps_probe, NULL, NULL);
    g_object_unref (sink);

    g_object_set (capsfilter, "caps", filter_caps, NULL);
    gst_caps_unref (filter_caps);

    gst_bin_add (GST_BIN (self), capsfilter);
    gst_element_sync_state_with_parent (capsfilter);
  }

  kms_tree_bin_set_input_element (tree_bin, rate);
  output_tee = kms_tree_bin_get_output_tee (tree_bin);
  if (self->priv->enc_type == X264) {
    gst_element_link_many (rate, convert, mediator, capsfilter, self->priv->enc,
        output_tee, NULL);
  } else {
    gst_element_link_many (rate, convert, mediator, self->priv->enc, output_tee,
        NULL);
  }

  return TRUE;
}

KmsEncTreeBin *
kms_enc_tree_bin_new (const GstCaps * caps, gint target_bitrate,
    gint min_bitrate, gint max_bitrate)
{
  KmsEncTreeBin *enc;

  enc = g_object_new (KMS_TYPE_ENC_TREE_BIN, NULL);
  enc->priv->max_bitrate = max_bitrate;
  enc->priv->min_bitrate = min_bitrate;

  target_bitrate = KMS_ENC_TREE_BIN_LIMIT (enc, target_bitrate);
  if (!kms_enc_tree_bin_configure (enc, caps, target_bitrate)) {
    g_object_unref (enc);
    return NULL;
  }

  return enc;
}

static void
kms_enc_tree_bin_init (KmsEncTreeBin * self)
{
  self->priv = KMS_ENC_TREE_BIN_GET_PRIVATE (self);

  self->priv->enc_sink = NULL;
  self->priv->remb_manager = NULL;

  self->priv->remb_bitrate = -1;
  self->priv->tag_bitrate = -1;

  self->priv->max_bitrate = G_MAXINT;
  self->priv->min_bitrate = 0;
}

static void
kms_enc_tree_bin_dispose (GObject * object)
{
  KmsEncTreeBin *self = KMS_ENC_TREE_BIN (object);

  GST_DEBUG_OBJECT (object, "dispose");
  if (self->priv->enc_sink) {
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
