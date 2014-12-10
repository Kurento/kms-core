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

#ifndef __SDP_H__
#define __SDP_H__

#include <gst/sdp/gstsdpmessage.h>

#define RTCP_FB "rtcp-fb"
#define RTCP_FB_FIR "ccm fir"
#define RTCP_FB_NACK "nack"
#define RTCP_FB_PLI "nack pli"
#define RTCP_FB_REMB "goog-remb"

typedef enum GstSDPDirection
{
  SENDONLY,
  RECVONLY,
  SENDRECV,
  INACTIVE
} GstSDPDirection;

GstSDPDirection sdp_utils_media_get_direction (const GstSDPMedia * media);
const gchar *sdp_utils_get_direction_str (GstSDPDirection direction);
guint sdp_utils_media_get_ssrc (const GstSDPMedia * media);

GstSDPResult sdp_utils_intersect_sdp_messages (const GstSDPMessage * offer,
    const GstSDPMessage * answer, GstSDPMessage ** offer_result,
    GstSDPMessage ** answer_result);

const gchar *sdp_utils_sdp_media_get_rtpmap (const GstSDPMedia * media,
    const gchar * format);

gchar *gst_sdp_media_format_get_encoding_name (const GstSDPMedia * media,
    const gchar * format);

void sdp_utils_set_max_video_recv_bw (GstSDPMessage * msg, gint max_video_recv_bw);

#endif /* __SDP_H__ */
