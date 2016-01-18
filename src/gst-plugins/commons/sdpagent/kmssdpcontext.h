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
#ifndef _KMS_SDP_CONTEXT_H_
#define _KMS_SDP_CONTEXT_H_

#include <gst/sdp/gstsdpmessage.h>

typedef enum  {
  IPV4,
  IPV6
} SdpIPv;

typedef struct _SdpMediaGroup SdpMediaGroup;
typedef struct _SdpMediaConfig SdpMediaConfig;
typedef struct _SdpMessageContext SdpMessageContext;

typedef enum  {
  KMS_SDP_UNDEFINED,
  KMS_SDP_OFFER,
  KMS_SDP_ANSWER
} KmsSdpMessageType;

const gchar * kms_sdp_message_context_ipv2str (SdpIPv ipv);

SdpMessageContext *kms_sdp_message_context_new (GError **error);
gboolean kms_sdp_message_context_set_origin(SdpMessageContext *ctx, const GstSDPOrigin *origin, GError **error);
const GstSDPOrigin * kms_sdp_message_context_get_origin (const SdpMessageContext *ctx);

SdpMessageContext * kms_sdp_message_context_ref (SdpMessageContext *ctx);
void kms_sdp_message_context_unref (SdpMessageContext *ctx);
SdpMessageContext *kms_sdp_message_context_new_from_sdp (GstSDPMessage *sdp, GError **error);
gboolean kms_sdp_message_context_set_common_session_attributes (SdpMessageContext *ctx, const GstSDPMessage *msg, GError **error);
SdpMediaConfig * kms_sdp_message_context_add_media (SdpMessageContext *ctx, GstSDPMedia *media, GError **error);
gint kms_sdp_media_config_get_id (SdpMediaConfig * mconf);
const gchar* kms_sdp_media_config_get_mid (SdpMediaConfig * mconf);
gboolean kms_sdp_media_config_is_rtcp_mux (SdpMediaConfig * mconf);
SdpMediaGroup * kms_sdp_media_config_get_group (SdpMediaConfig * mconf);
GstSDPMedia * kms_sdp_media_config_get_sdp_media (SdpMediaConfig * mconf);
gboolean kms_sdp_media_config_is_inactive (SdpMediaConfig * mconf);
gint kms_sdp_media_config_get_abs_send_time_id (SdpMediaConfig * mconf);
GstSDPMessage * kms_sdp_message_context_pack (SdpMessageContext *ctx, GError **error);
SdpMediaGroup * kms_sdp_message_context_create_group (SdpMessageContext *ctx, guint gid);
gboolean kms_sdp_message_context_has_groups (SdpMessageContext *ctx);
gint kms_sdp_media_group_get_id (SdpMediaGroup *group);
SdpMediaGroup * kms_sdp_message_context_get_group (SdpMessageContext *ctx, guint gid);
gboolean kms_sdp_message_context_add_media_to_group (SdpMediaGroup *group, SdpMediaConfig *media, GError **error);
gboolean kms_sdp_message_context_remove_media_from_group (SdpMediaGroup *group, guint id, GError **error);
gboolean kms_sdp_message_context_parse_groups_from_offer (SdpMessageContext *ctx, const GstSDPMessage *offer, GError **error);
GstSDPMessage * kms_sdp_message_context_get_sdp_message (SdpMessageContext *ctx);
GSList * kms_sdp_message_context_get_medias (SdpMessageContext *ctx);
void kms_sdp_message_context_set_type (SdpMessageContext *ctx, KmsSdpMessageType type);
KmsSdpMessageType kms_sdp_message_context_get_type (SdpMessageContext *ctx);

#endif /* _KMS_SDP_CONTEXT_ */
