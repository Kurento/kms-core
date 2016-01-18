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

#include "kmssdpagentcommon.h"

static void
kms_sdp_agent_common_destroy_sdp_handler (KmsSdpHandler * handler)
{
  g_free (handler->media);
  g_slist_free (handler->groups);
  g_clear_object (&handler->handler);

  g_slice_free (KmsSdpHandler, handler);
}

KmsSdpHandler *
kms_sdp_agent_common_new_sdp_handler (guint id, const gchar * media,
    KmsSdpMediaHandler * handler)
{
  KmsSdpHandler *sdp_handler;

  sdp_handler = g_slice_new0 (KmsSdpHandler);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (sdp_handler),
      (GDestroyNotify) kms_sdp_agent_common_destroy_sdp_handler);

  sdp_handler->id = id;
  sdp_handler->media = g_strdup (media);
  sdp_handler->handler = handler;

  return sdp_handler;
}

KmsSdpHandler *
kms_sdp_agent_common_ref_sdp_handler (KmsSdpHandler * handler)
{
  return (KmsSdpHandler *) kms_ref_struct_ref (KMS_REF_STRUCT_CAST (handler));
}

void
kms_sdp_agent_common_unref_sdp_handler (KmsSdpHandler * handler)
{
  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (handler));
}
