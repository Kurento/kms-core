/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#include <kmsrecordingprofile.h>

static GstEncodingContainerProfile *
kms_recording_profile_create_webm_profile (gboolean has_audio,
    gboolean has_video)
{
  GstEncodingContainerProfile *cprof;
  GstCaps *pc;

  if (has_video)
    pc = gst_caps_from_string ("video/webm");
  else
    pc = gst_caps_from_string ("audio/webm");

  cprof = gst_encoding_container_profile_new ("Webm", NULL, pc, NULL);
  gst_caps_unref (pc);

  if (has_audio) {
    GstCaps *ac = gst_caps_from_string ("audio/x-opus");

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_audio_profile_new (ac, NULL, NULL, 0));

    gst_caps_unref (ac);
  }

  if (has_video) {
    GstCaps *vc = gst_caps_from_string ("video/x-vp8");

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_video_profile_new (vc, NULL, NULL, 0));

    gst_caps_unref (vc);
  }

  return cprof;
}

static GstEncodingContainerProfile *
kms_recording_profile_create_mp4_profile (gboolean has_audio,
    gboolean has_video)
{
  GstEncodingContainerProfile *cprof;
  GstCaps *pc;

  pc = gst_caps_from_string ("video/quicktime, variant=(string)iso");

  cprof = gst_encoding_container_profile_new ("Mp4", NULL, pc, NULL);
  gst_caps_unref (pc);

  if (has_audio) {
    GstCaps *ac = gst_caps_from_string ("audio/mpeg,mpegversion=1,layer=3");

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_audio_profile_new (ac, NULL, NULL, 0));

    gst_caps_unref (ac);
  }

  if (has_video) {
    GstCaps *vc = gst_caps_from_string ("video/x-h264, "
        "stream-format=(string)avc, alignment=(string)au");

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_video_profile_new (vc, NULL, NULL, 0));

    gst_caps_unref (vc);
  }

  return cprof;
}

static GstEncodingContainerProfile *
kms_recording_profile_create_jpeg_profile (gboolean has_audio,
    gboolean has_video)
{
  GstEncodingContainerProfile *cprof;
  GstCaps *pc;

  pc = gst_caps_from_string ("image/jpeg");
  cprof = gst_encoding_container_profile_new ("jpeg", NULL, pc, NULL);
  gst_caps_unref (pc);

  if (has_video) {
    GstCaps *vc = gst_caps_from_string ("image/jpeg");

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_video_profile_new (vc, NULL, NULL, 0));

    gst_caps_unref (vc);
  }

  return cprof;
}

static GstEncodingContainerProfile *
kms_recording_profile_create_ksr_profile (gboolean has_audio,
    gboolean has_video)
{
  GstEncodingContainerProfile *cprof;
  GstPadTemplate *templ;
  GstElement *mux;
  GstCaps *pc;

  pc = gst_caps_from_string ("application/x-ksr");
  cprof = gst_encoding_container_profile_new ("Ksr", NULL, pc, NULL);
  gst_caps_unref (pc);

  /* Use matroska caps to define this profile */
  mux = gst_element_factory_make ("matroskamux", NULL);

  if (has_audio) {
    GstCaps *ac;

    templ =
        gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (mux),
        "audio_%u");
    ac = gst_pad_template_get_caps (templ);

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_audio_profile_new (ac, NULL, NULL, 0));

    gst_caps_unref (ac);
  }

  if (has_video) {
    GstCaps *vc;

    templ =
        gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (mux),
        "video_%u");
    vc = gst_pad_template_get_caps (templ);

    gst_encoding_container_profile_add_profile (cprof, (GstEncodingProfile *)
        gst_encoding_video_profile_new (vc, NULL, NULL, 0));

    gst_caps_unref (vc);
  }

  g_object_unref (mux);

  return cprof;
}

GstEncodingContainerProfile *
kms_recording_profile_create_profile (KmsRecordingProfile profile,
    gboolean has_audio, gboolean has_video)
{
  switch (profile) {
    case KMS_RECORDING_PROFILE_WEBM:
      return kms_recording_profile_create_webm_profile (has_audio, has_video);
    case KMS_RECORDING_PROFILE_MP4:
      return kms_recording_profile_create_mp4_profile (has_audio, has_video);
    case KMS_RECORDING_PROFILE_WEBM_VIDEO_ONLY:
      return kms_recording_profile_create_webm_profile (FALSE, has_video);
    case KMS_RECORDING_PROFILE_WEBM_AUDIO_ONLY:
      return kms_recording_profile_create_webm_profile (has_audio, FALSE);
    case KMS_RECORDING_PROFILE_MP4_VIDEO_ONLY:
      return kms_recording_profile_create_mp4_profile (FALSE, has_video);
    case KMS_RECORDING_PROFILE_MP4_AUDIO_ONLY:
      return kms_recording_profile_create_mp4_profile (has_audio, FALSE);
    case KMS_RECORDING_PROFILE_KSR:
      return kms_recording_profile_create_ksr_profile (has_audio, has_video);
    case KMS_RECORDING_PROFILE_JPEG:
      return kms_recording_profile_create_jpeg_profile (has_audio, has_video);
    default:
      GST_WARNING ("Invalid recording profile");
      return NULL;
  }

}

gboolean
kms_recording_profile_supports_type (KmsRecordingProfile profile,
    KmsElementPadType type)
{
  if (type == KMS_ELEMENT_PAD_TYPE_DATA) {
    return FALSE;
  }

  switch (profile) {
    case KMS_RECORDING_PROFILE_WEBM:
    case KMS_RECORDING_PROFILE_MP4:
      return TRUE;
    case KMS_RECORDING_PROFILE_WEBM_VIDEO_ONLY:
    case KMS_RECORDING_PROFILE_MP4_VIDEO_ONLY:
      return type == KMS_ELEMENT_PAD_TYPE_VIDEO;
    case KMS_RECORDING_PROFILE_WEBM_AUDIO_ONLY:
    case KMS_RECORDING_PROFILE_MP4_AUDIO_ONLY:
      return type == KMS_ELEMENT_PAD_TYPE_AUDIO;
    case KMS_RECORDING_PROFILE_KSR:
      return TRUE;
    case KMS_RECORDING_PROFILE_JPEG:
      return type == KMS_ELEMENT_PAD_TYPE_VIDEO;
    default:
      return FALSE;
  }
}
