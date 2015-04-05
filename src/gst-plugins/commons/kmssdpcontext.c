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

#include "sdp_utils.h"
#include "kmsutils.h"
#include "kmssdpagent.h"
#include "kmssdpcontext.h"

#define GST_CAT_DEFAULT sdp_context
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "sdp_context"

#define ORIGIN_ATTR_NETTYPE "IN"
#define ORIGIN_ATTR_ADDR_TYPE_IP4 "IP4"
#define ORIGIN_ATTR_ADDR_TYPE_IP6 "IP6"
#define DEFAULT_IP4_ADDR "0.0.0.0"
#define DEFAULT_IP6_ADDR "::"

struct _SdpMediaGroup
{
  guint id;
  GSList *medias;
};

struct _SdpMediaConfig
{
  guint id;
  gchar *mid;
  GstSDPMedia *media;
};

struct _SdpMessageContext
{
  GstSDPMessage *msg;
  GSList *medias;               /* list of SdpMediaConfigs */
  GHashTable *mids;
  GSList *groups;
};

static SdpMediaGroup *
kms_sdp_context_new_media_group (guint gid)
{
  SdpMediaGroup *group;

  group = g_slice_new0 (SdpMediaGroup);
  group->id = gid;

  return group;
}

static void
kms_sdp_context_destroy_media_group (SdpMediaGroup * group)
{
  g_slist_free (group->medias);

  g_slice_free (SdpMediaGroup, group);
}

static SdpMediaConfig *
kms_sdp_context_new_media_config (guint id, gchar * mid, GstSDPMedia * media)
{
  SdpMediaConfig *mconf;

  mconf = g_slice_new0 (SdpMediaConfig);
  mconf->id = id;
  mconf->mid = mid;
  mconf->media = media;

  return mconf;
}

static void
kms_sdp_context_destroy_media_config (SdpMediaConfig * mconf)
{
  if (mconf->media != NULL) {
    gst_sdp_media_free (mconf->media);
  }

  g_free (mconf->mid);

  g_slice_free (SdpMediaConfig, mconf);
}

static const gchar *
get_attr_addr_type (SdpIPv ipv)
{
  switch (ipv) {
    case IPV4:
      return ORIGIN_ATTR_ADDR_TYPE_IP4;
    case IPV6:
      return ORIGIN_ATTR_ADDR_TYPE_IP6;
    default:
      return NULL;
  }
}

static const gchar *
get_default_ipv_addr (SdpIPv ipv)
{
  switch (ipv) {
    case IPV4:
      return DEFAULT_IP4_ADDR;
    case IPV6:
      return DEFAULT_IP6_ADDR;
    default:
      return NULL;
  }
}

static gboolean
kms_sdp_message_context_set_default_session_attributes (GstSDPMessage * msg,
    SdpIPv ipv, GError ** error)
{
  const gchar *addrtype, *addr, *err_attr;

  addrtype = get_attr_addr_type (ipv);
  if (addrtype == NULL) {
    err_attr = "ip version";
    goto error;
  }

  addr = get_default_ipv_addr (ipv);

  if (gst_sdp_message_set_version (msg, "0") != GST_SDP_OK) {
    err_attr = "version";
    goto error;
  }

  if (gst_sdp_message_set_origin (msg, "-", "0", "0", ORIGIN_ATTR_NETTYPE,
          addrtype, addr) != GST_SDP_OK) {
    err_attr = "origin";
    goto error;
  }

  if (gst_sdp_message_set_session_name (msg,
          "Kurento Media Server") != GST_SDP_OK) {
    err_attr = "session";
    goto error;
  }

  if (gst_sdp_message_set_connection (msg, ORIGIN_ATTR_NETTYPE, addrtype,
          addr, 0, 0) != GST_SDP_OK) {
    err_attr = "connection";
    goto error;
  }

  return TRUE;

error:
  g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
      "Can not set attr: %s", err_attr);

  return FALSE;
}

SdpMessageContext *
kms_sdp_context_new_message_context (SdpIPv ipv, GError ** error)
{
  SdpMessageContext *ctx;

  ctx = g_slice_new0 (SdpMessageContext);
  gst_sdp_message_new (&ctx->msg);
  ctx->mids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) kms_utils_destroy_guint);

  if (!kms_sdp_message_context_set_default_session_attributes (ctx->msg, ipv,
          error)) {
    kms_sdp_context_destroy_message_context (ctx);
    return NULL;
  }

  return ctx;
}

static gboolean
intersect_session_attr (const GstSDPAttribute * attr, gpointer user_data)
{
  SdpMessageContext *ctx = user_data;
  guint i, len;

  len = gst_sdp_message_attributes_len (ctx->msg);

  for (i = 0; i < len; i++) {
    const GstSDPAttribute *a;

    a = gst_sdp_message_get_attribute (ctx->msg, i);

    if (g_strcmp0 (attr->key, a->key) == 0 &&
        g_strcmp0 (attr->value, a->value) == 0) {
      return FALSE;
    }
  }

  return gst_sdp_message_add_attribute (ctx->msg, attr->key,
      attr->value) == GST_SDP_OK;
}

gboolean
kms_sdp_context_set_common_session_attributes (SdpMessageContext * ctx,
    const GstSDPMessage * msg, GError ** error)
{
  if (!sdp_utils_intersect_session_attributes (msg, intersect_session_attr,
          ctx)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not interset session attributes");
    return FALSE;
  }

  return TRUE;
}

void
kms_sdp_context_destroy_message_context (SdpMessageContext * ctx)
{
  if (ctx->msg != NULL) {
    gst_sdp_message_free (ctx->msg);
  }

  g_hash_table_unref (ctx->mids);

  g_slist_free_full (ctx->medias,
      (GDestroyNotify) kms_sdp_context_destroy_media_config);

  g_slist_free_full (ctx->groups,
      (GDestroyNotify) kms_sdp_context_destroy_media_group);

  g_slice_free (SdpMessageContext, ctx);
}

SdpMediaConfig *
kms_sdp_context_add_media (SdpMessageContext * ctx, GstSDPMedia * media)
{
  SdpMediaConfig *mconf;
  const gchar *media_type;
  gchar *mid;
  guint *counter;

  media_type = gst_sdp_media_get_media (media);
  counter = g_hash_table_lookup (ctx->mids, media_type);

  if (counter == NULL) {
    /* No stored medias of this type yet */
    counter = g_slice_new0 (guint);
    g_hash_table_insert (ctx->mids, g_strdup (media_type), counter);
  }

  mid = g_strdup_printf ("%s%u", media_type, (*counter)++);

  mconf = kms_sdp_context_new_media_config (g_slist_length (ctx->medias), mid,
      media);

  ctx->medias = g_slist_append (ctx->medias, mconf);

  return mconf;
}

static void
add_media_to_sdp_message (SdpMediaConfig * mconf, GstSDPMessage * msg)
{
  if (gst_sdp_message_get_attribute_val (msg, "group") != NULL &&
      gst_sdp_media_get_attribute_val (mconf->media, "mid") == NULL) {
    /* When group attribute is present, the mid attribute */
    /* in media is mandatory */
    gst_sdp_media_add_attribute (mconf->media, "mid", mconf->mid);
  }

  gst_sdp_message_add_media (msg, mconf->media);
}

static void
add_group_to_sdp_message (SdpMediaGroup * group, GstSDPMessage * msg)
{
  gchar *val;
  GSList *l;

  if (g_slist_length (group->medias) <= 0) {
    /* No medias in group */
    return;
  }

  val = g_strdup ("BUNDLE");

  for (l = group->medias; l != NULL; l = l->next) {
    SdpMediaConfig *mconf = l->data;
    gchar *tmp;

    tmp = val;
    val = g_strdup_printf ("%s %s", tmp, mconf->mid);
    g_free (tmp);
  }

  gst_sdp_message_add_attribute (msg, "group", val);
  g_free (val);
}

GstSDPMessage *
sdp_mesage_context_pack (SdpMessageContext * ctx, GError ** error)
{
  GstSDPMessage *msg;
  gchar *sdp_str;

  gst_sdp_message_new (&msg);

  /* Context's message only stores media session attributes */
  sdp_str = gst_sdp_message_as_text (ctx->msg);

  if (gst_sdp_message_parse_buffer ((const guint8 *) sdp_str, -1,
          msg) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Can not create SDP message");
    g_free (sdp_str);
    return NULL;
  }

  g_free (sdp_str);

  /* Add group attributes */
  g_slist_foreach (ctx->groups, (GFunc) add_group_to_sdp_message, msg);

  /* Append medias to the message */
  g_slist_foreach (ctx->medias, (GFunc) add_media_to_sdp_message, msg);

  return msg;
}

SdpMediaGroup *
kms_sdp_context_create_group (SdpMessageContext * ctx, guint gid)
{
  SdpMediaGroup *group;

  group = kms_sdp_context_new_media_group (gid);
  ctx->groups = g_slist_append (ctx->groups, group);

  return group;
}

SdpMediaGroup *
kms_sdp_context_get_group (SdpMessageContext * ctx, guint gid)
{
  GSList *l;

  for (l = ctx->groups; l != NULL; l = l->next) {
    SdpMediaGroup *group = l->data;

    if (group->id == gid) {
      return group;
    }
  }

  return NULL;
}

gboolean
kms_sdp_context_add_media_to_group (SdpMediaGroup * group,
    SdpMediaConfig * media)
{
  GSList *l;

  for (l = group->medias; l != NULL; l = l->next) {
    SdpMediaConfig *mconf = l->data;

    if (mconf->id == media->id) {
      return FALSE;
    }
  }

  group->medias = g_slist_append (group->medias, media);

  return TRUE;
}

static void init_debug (void) __attribute__ ((constructor));

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}
