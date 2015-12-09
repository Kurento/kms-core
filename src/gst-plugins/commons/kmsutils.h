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
#ifndef __KMS_UTILS_H__
#define __KMS_UTILS_H__

#include "gst/gst.h"
#include "kmsmediatype.h"
#include "sdpagent/kmssdpcontext.h"

G_BEGIN_DECLS

typedef void (*KmsPadIterationAction) (GstPad * pad, gpointer data);
typedef void (*KmsPadCallback) (GstPad * pad, gpointer data);

void kms_element_for_each_src_pad (GstElement * element,
  KmsPadCallback action, gpointer data);

void kms_utils_debug_graph_delay (GstBin * bin, guint interval);
gboolean kms_is_valid_uri (const gchar * url);

gboolean gst_element_sync_state_with_parent_target_state (GstElement * element);

/* Caps */
gboolean kms_utils_caps_are_audio (const GstCaps * caps);
gboolean kms_utils_caps_are_video (const GstCaps * caps);

gboolean kms_utils_caps_are_raw (const GstCaps * caps);

GstElement * kms_utils_create_convert_for_caps (const GstCaps * caps);
GstElement * kms_utils_create_mediator_element (const GstCaps * caps);
GstElement * kms_utils_create_rate_for_caps (const GstCaps * caps);

/* key frame management */
void kms_utils_drop_until_keyframe (GstPad *pad, gboolean all_headers);
void kms_utils_manage_gaps (GstPad *pad);
void kms_utils_control_key_frames_request_duplicates (GstPad *pad);

/* Pad blocked action */
void kms_utils_execute_with_pad_blocked (GstPad * pad, gboolean drop, KmsPadCallback func, gpointer userData);

/* REMB event */
GstEvent * kms_utils_remb_event_upstream_new (guint bitrate, guint ssrc);
gboolean kms_utils_remb_event_upstream_parse (GstEvent *event, guint *bitrate, guint *ssrc);

typedef struct _RembEventManager RembEventManager;
typedef void (*RembBitrateUpdatedCallback) (RembEventManager * manager, guint bitrate, gpointer user_data);
RembEventManager * kms_utils_remb_event_manager_create (GstPad *pad);
void kms_utils_remb_event_manager_destroy (RembEventManager * manager);
void kms_utils_remb_event_manager_pointer_destroy (gpointer manager);
guint kms_utils_remb_event_manager_get_min (RembEventManager * manager);
void kms_utils_remb_event_manager_set_callback (RembEventManager * manager, RembBitrateUpdatedCallback cb, gpointer data, GDestroyNotify destroy_notify);
void kms_utils_remb_event_manager_set_clear_interval (RembEventManager * manager, GstClockTime interval);
GstClockTime kms_utils_remb_event_manager_get_clear_interval (RembEventManager * manager);

/* time */
GstClockTime kms_utils_get_time_nsecs ();

/* RTP connection */
gchar * kms_utils_create_connection_name_from_media_config (SdpMediaConfig * mconf);

gboolean kms_utils_contains_proto (const gchar *search_term, const gchar *proto);
const GstStructure * kms_utils_get_structure_by_name (const GstStructure *str, const gchar *name);

void kms_utils_set_uuid (GObject *obj);
const gchar * kms_utils_get_uuid (GObject *obj);

const char * kms_utils_media_type_to_str (KmsMediaType type);

/* Type destroying */
#define KMS_UTILS_DESTROY_H(type) void kms_utils_destroy_##type (type * data);
KMS_UTILS_DESTROY_H (guint64)
KMS_UTILS_DESTROY_H (gsize)
KMS_UTILS_DESTROY_H (GstClockTime)
KMS_UTILS_DESTROY_H (gfloat)
KMS_UTILS_DESTROY_H (guint)

G_END_DECLS

#endif /* __KMS_UTILS_H__ */
