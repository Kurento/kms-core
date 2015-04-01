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

typedef struct _SdpMediaGroup SdpMediaGroup;
typedef struct _SdpMediaConfig SdpMediaConfig;
typedef struct _SdpMessageContext SdpMessageContext;

SdpMessageContext *kms_sdp_context_new_message_context (void);
void kms_sdp_context_destroy_message_context (SdpMessageContext *ctx);
SdpMediaConfig * kms_sdp_context_add_media (SdpMessageContext *ctx, GstSDPMedia *media);
GstSDPMessage * sdp_mesage_context_pack (SdpMessageContext *ctx);
SdpMediaGroup * kms_sdp_context_create_group (SdpMessageContext *ctx, guint gid);
SdpMediaGroup * kms_sdp_context_get_group (SdpMessageContext *ctx, guint gid);
gboolean kms_sdp_context_add_media_to_group (SdpMediaGroup *group, SdpMediaConfig *media);

#endif /* _KMS_SDP_CONTEXT_ */
