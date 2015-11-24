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

#ifndef __KMS_RECORDING_PROFILE_H__
#define __KMS_RECORDING_PROFILE_H__

#include <glib.h>

#include <gst/pbutils/encoding-profile.h>
#include "kmselement.h"

G_BEGIN_DECLS

typedef enum
{
  KMS_RECORDING_PROFILE_NONE = -1,
  KMS_RECORDING_PROFILE_WEBM,
  KMS_RECORDING_PROFILE_MP4,
  KMS_RECORDING_PROFILE_WEBM_VIDEO_ONLY,
  KMS_RECORDING_PROFILE_WEBM_AUDIO_ONLY,
  KMS_RECORDING_PROFILE_MP4_VIDEO_ONLY,
  KMS_RECORDING_PROFILE_MP4_AUDIO_ONLY,
  KMS_RECORDING_PROFILE_KSR
} KmsRecordingProfile;

GstEncodingContainerProfile * kms_recording_profile_create_profile (
    KmsRecordingProfile profile, gboolean has_audio, gboolean has_video);

gboolean kms_recording_profile_supports_type (KmsRecordingProfile profile,
    KmsElementPadType type);

G_END_DECLS
#endif /* __KMS_RECORDING_PROFILE_H__ */
