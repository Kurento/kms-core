/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "kmsrefstruct.h"
#include "sdp_utils.h"
#include "constants.h"
#include "kmsutils.h"
#include "kmssdpagent.h"
#include "kmssdpcontext.h"
#include <stdlib.h>

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
  SdpMediaGroup *group;
  GstSDPMedia *media;
};

struct _SdpMessageContext
{
  KmsRefStruct ref;
  KmsSdpMessageType type;
  GstSDPMessage *msg;
  GSList *medias;               /* list of SdpMediaConfigs */
  GSList *grouped_medias;
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

const gchar *
kms_sdp_message_context_ipv2str (SdpIPv ipv)
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

static gboolean
kms_sdp_message_context_set_default_session_attributes (GstSDPMessage * msg,
    GError ** error)
{
  const gchar *err_attr;

  if (gst_sdp_message_set_version (msg, "0") != GST_SDP_OK) {
    err_attr = "version";
    goto error;
  }

  if (gst_sdp_message_set_session_name (msg,
          "Kurento Media Server") != GST_SDP_OK) {
    err_attr = "session";
    goto error;
  }

  return TRUE;

error:
  g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_INVALID_PARAMETER,
      "Can not set attr: %s", err_attr);

  return FALSE;
}

static void
kms_sdp_message_context_destroy (SdpMessageContext * ctx)
{
  if (ctx->msg != NULL) {
    gst_sdp_message_free (ctx->msg);
  }

  g_hash_table_unref (ctx->mids);

  g_slist_free_full (ctx->grouped_medias,
      (GDestroyNotify) kms_sdp_context_destroy_media_config);

  g_slist_free_full (ctx->medias,
      (GDestroyNotify) kms_sdp_context_destroy_media_config);

  g_slist_free_full (ctx->groups,
      (GDestroyNotify) kms_sdp_context_destroy_media_group);

  g_slice_free (SdpMessageContext, ctx);
}

SdpMessageContext *
kms_sdp_message_context_new (GError ** error)
{
  SdpMessageContext *ctx;

  ctx = g_slice_new0 (SdpMessageContext);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (ctx),
      (GDestroyNotify) kms_sdp_message_context_destroy);

  gst_sdp_message_new (&ctx->msg);
  ctx->mids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) kms_utils_destroy_guint);

  if (!kms_sdp_message_context_set_default_session_attributes (ctx->msg, error)) {
    kms_sdp_message_context_destroy (ctx);
    return NULL;
  }

  return ctx;
}

SdpMessageContext *
kms_sdp_message_context_ref (SdpMessageContext * ctx)
{
  return (SdpMessageContext *) kms_ref_struct_ref (KMS_REF_STRUCT_CAST (ctx));
}

void
kms_sdp_message_context_unref (SdpMessageContext * ctx)
{
  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (ctx));
}

static gboolean
intersect_session_attr (const GstSDPAttribute * attr, gpointer user_data)
{
  SdpMessageContext *ctx = user_data;
  guint i, len;

  if (g_strcmp0 (attr->key, "group") == 0) {
    /* Exclude group attributes so they are managed indepently */
    return TRUE;
  }

  /* Check that this attribute is already in the message */

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
kms_sdp_message_context_set_common_session_attributes (SdpMessageContext * ctx,
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

static gboolean
configure_pending_mediaconfig (SdpMessageContext * ctx, GstSDPMedia * media,
    SdpMediaConfig ** mconf)
{
  const gchar *val;
  GSList *l;

  val = gst_sdp_media_get_attribute_val (media, "mid");

  if (val == NULL) {
    /* No grouped */
    return FALSE;
  }

  for (l = ctx->groups; l != NULL; l = l->next) {
    SdpMediaGroup *group = l->data;
    GSList *ll;

    for (ll = group->medias; ll != NULL; ll = ll->next) {
      SdpMediaConfig *m;

      m = ll->data;

      if (m->media != NULL) {
        continue;
      }

      if (g_strcmp0 (m->mid, val) == 0) {
        m->media = media;
        m->group = group;
        *mconf = m;
        return TRUE;
      }
    }
  }

  return FALSE;
}

static void
configure_new_mediaconfig (SdpMessageContext * ctx, GstSDPMedia * media,
    SdpMediaConfig ** mconf)
{
  gchar *mid;

  mid = g_strdup (gst_sdp_media_get_attribute_val (media, "mid"));
  if (mid == NULL) {
    const gchar *media_type;
    guint *counter;

    media_type = gst_sdp_media_get_media (media);
    counter = g_hash_table_lookup (ctx->mids, media_type);

    if (counter == NULL) {
      /* No stored medias of this type yet */
      counter = g_slice_new0 (guint);
      g_hash_table_insert (ctx->mids, g_strdup (media_type), counter);
    }

    mid = g_strdup_printf ("%s%u", media_type, (*counter)++);
  }

  *mconf = kms_sdp_context_new_media_config (g_slist_length (ctx->medias), mid,
      media);
}

SdpMediaConfig *
kms_sdp_message_context_add_media (SdpMessageContext * ctx, GstSDPMedia * media,
    GError ** error)
{
  SdpMediaConfig *mconf;

  if (ctx->type == KMS_SDP_ANSWER && g_slist_length (ctx->groups) > 0 &&
      gst_sdp_media_get_attribute_val (media, "mid") == NULL) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Missing mid attribute for media");
    return NULL;
  }

  if (configure_pending_mediaconfig (ctx, media, &mconf)) {
    ctx->grouped_medias = g_slist_remove (ctx->grouped_medias, mconf);
    /* Set proper id */
    mconf->id = g_slist_length (ctx->medias);
    goto end;
  }

  configure_new_mediaconfig (ctx, media, &mconf);

end:
  ctx->medias = g_slist_append (ctx->medias, mconf);

  return mconf;
}

gint
kms_sdp_media_config_get_id (SdpMediaConfig * mconf)
{
  return mconf->id;
}

const gchar *
kms_sdp_media_config_get_mid (SdpMediaConfig * mconf)
{
  return mconf->mid;
}

gboolean
kms_sdp_media_config_is_rtcp_mux (SdpMediaConfig * mconf)
{
  return gst_sdp_media_get_attribute_val (mconf->media, "rtcp-mux") != NULL;
}

SdpMediaGroup *
kms_sdp_media_config_get_group (SdpMediaConfig * mconf)
{
  return mconf->group;
}

GstSDPMedia *
kms_sdp_media_config_get_sdp_media (SdpMediaConfig * mconf)
{
  return mconf->media;
}

gboolean
kms_sdp_media_config_is_inactive (SdpMediaConfig * mconf)
{
  return (gst_sdp_media_get_attribute_val (mconf->media, "inactive") != NULL)
      || (gst_sdp_media_get_port (mconf->media) == 0);
}

gint
kms_sdp_media_config_get_abs_send_time_id (SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  guint a;

  for (a = 0;; a++) {
    const gchar *attr;
    gchar **tokens;

    attr = gst_sdp_media_get_attribute_val_n (media, EXT_MAP, a);
    if (attr == NULL) {
      break;
    }

    tokens = g_strsplit (attr, " ", 0);
    if (g_strcmp0 (RTP_HDR_EXT_ABS_SEND_TIME_URI, tokens[1]) == 0) {
      gint ret = atoi (tokens[0]);

      g_strfreev (tokens);
      return ret;
    }

    g_strfreev (tokens);
  }

  return -1;
}

static gboolean
add_media_to_sdp_message (SdpMediaConfig * mconf, GstSDPMessage * msg,
    GError ** error)
{
  GstSDPMedia *cpy;

  if (gst_sdp_media_get_attribute_val (mconf->media, "mid") == NULL) {
    /* When group attribute is present, the mid attribute */
    /* in media is mandatory */
    gst_sdp_media_add_attribute (mconf->media, "mid", mconf->mid);
  }

  if (gst_sdp_media_copy (mconf->media, &cpy) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "can not create media entry");
    return FALSE;
  }

  gst_sdp_message_add_media (msg, cpy);
  gst_sdp_media_free (cpy);

  return TRUE;
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

    if (mconf->media == NULL || gst_sdp_media_get_port (mconf->media) == 0) {
      /* Move this media out the group */
      continue;
    }

    tmp = val;
    val = g_strdup_printf ("%s %s", tmp, mconf->mid);
    g_free (tmp);
  }

  gst_sdp_message_add_attribute (msg, "group", val);
  g_free (val);
}

GstSDPMessage *
kms_sdp_message_context_pack (SdpMessageContext * ctx, GError ** error)
{
  GstSDPMessage *msg;
  gchar *sdp_str;
  GSList *l;

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
  for (l = ctx->medias; l != NULL; l = g_slist_next (l)) {
    if (!add_media_to_sdp_message (l->data, msg, error)) {
      gst_sdp_message_free (msg);
      return NULL;
    }
  }

  return msg;
}

SdpMediaGroup *
kms_sdp_message_context_create_group (SdpMessageContext * ctx, guint gid)
{
  SdpMediaGroup *group;

  group = kms_sdp_context_new_media_group (gid);
  ctx->groups = g_slist_append (ctx->groups, group);

  return group;
}

gboolean
kms_sdp_message_context_has_groups (SdpMessageContext * ctx)
{
  return g_slist_length (ctx->groups) > 0;
}

gint
kms_sdp_media_group_get_id (SdpMediaGroup * group)
{
  return group->id;
}

SdpMediaGroup *
kms_sdp_message_context_get_group (SdpMessageContext * ctx, guint gid)
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
kms_sdp_message_context_add_media_to_group (SdpMediaGroup * group,
    SdpMediaConfig * media, GError ** error)
{
  if (media->group != NULL) {
    g_set_error (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "Media already belongs to group (%d)", group->id);
    return FALSE;
  }

  group->medias = g_slist_append (group->medias, media);
  media->group = group;

  return TRUE;
}

static gint
compare_media_config_by_id (SdpMediaConfig * mconf, guint * id)
{
  return *id - mconf->id;
}

gboolean
kms_sdp_message_context_remove_media_from_group (SdpMediaGroup * group,
    guint id, GError ** error)
{
  GSList *l;

  l = g_slist_find_custom (group->medias, &id,
      (GCompareFunc) compare_media_config_by_id);

  if (l == NULL) {
    return FALSE;
  }

  group->medias = g_slist_remove (group->medias, l->data);

  return TRUE;
}

gboolean
kms_sdp_message_context_parse_groups_from_offer (SdpMessageContext * ctx,
    const GstSDPMessage * offer, GError ** error)
{
  guint i, gid = 0;

  for (i = 0;; i++) {
    SdpMediaGroup *mgroup;
    gboolean is_bundle;
    const gchar *val;
    gchar **grp;
    guint j;

    val = gst_sdp_message_get_attribute_val_n (offer, "group", i);

    if (val == NULL) {
      return TRUE;
    }

    grp = g_strsplit (val, " ", 0);
    is_bundle = g_strcmp0 (grp[0] /* group type */ , "BUNDLE") == 0;

    if (!is_bundle) {
      GST_WARNING ("Group '%s' is not supported", grp[0]);
      g_strfreev (grp);
      continue;
    }

    mgroup = kms_sdp_message_context_create_group (ctx, gid++);
    for (j = 1; grp[j] != NULL; j++) {
      SdpMediaConfig *mconf;

      mconf = kms_sdp_context_new_media_config (0, g_strdup (grp[j]), NULL);

      ctx->grouped_medias = g_slist_append (ctx->grouped_medias, mconf);
      if (!kms_sdp_message_context_add_media_to_group (mgroup, mconf, error)) {
        g_strfreev (grp);
        return FALSE;
      }
    }

    g_strfreev (grp);
  }
}

struct SdpMediaContextData
{
  SdpMessageContext *ctx;
  GError **err;
};

static gboolean
add_media_context (const GstSDPMedia * media, struct SdpMediaContextData *data)
{
  SdpMessageContext *ctx = data->ctx;
  GstSDPMedia *cpy;

  if (gst_sdp_media_copy (media, &cpy) != GST_SDP_OK) {
    g_set_error_literal (data->err, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_UNEXPECTED_ERROR, "Can not copy media entry");
    return FALSE;
  }

  if (kms_sdp_message_context_add_media (ctx, cpy, data->err) == NULL) {
    return FALSE;
  }

  return TRUE;
}

static gboolean
copy_session_attributes (SdpMessageContext * ctx, const GstSDPMessage * msg,
    GError ** error)
{
  const GstSDPOrigin *o1;
  const GstSDPConnection *conn;
  const gchar *s;

  o1 = gst_sdp_message_get_origin (msg);

  if (gst_sdp_message_set_origin (ctx->msg, o1->username, o1->sess_id,
          o1->sess_version, o1->nettype, o1->addrtype,
          o1->addr) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set origin");

    return FALSE;
  }

  s = gst_sdp_message_get_session_name (msg);
  if (gst_sdp_message_set_session_name (ctx->msg, s) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set session name");
    return FALSE;
  }

  conn = gst_sdp_message_get_connection (msg);
  gst_sdp_message_set_connection (ctx->msg, conn->nettype, conn->addrtype,
      conn->address, conn->ttl, conn->addr_number);

  if (!sdp_utils_intersect_session_attributes (msg, intersect_session_attr,
          ctx)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not interset session attributes");
    return FALSE;
  }

  return TRUE;
}

SdpMessageContext *
kms_sdp_message_context_new_from_sdp (GstSDPMessage * sdp, GError ** error)
{
  struct SdpMediaContextData data;
  SdpMessageContext *ctx;
  const GstSDPOrigin *o;

  ctx = kms_sdp_message_context_new (error);
  if (ctx == NULL) {
    return NULL;
  }

  o = gst_sdp_message_get_origin (sdp);

  if (!kms_sdp_message_context_set_origin (ctx, o, error)) {
    goto error;
  }

  if (!kms_sdp_message_context_parse_groups_from_offer (ctx, sdp, error)) {
    goto error;
  }

  if (!copy_session_attributes (ctx, sdp, error)) {
    goto error;
  }

  data.ctx = ctx;
  data.err = error;

  if (!sdp_utils_for_each_media (sdp, (GstSDPMediaFunc) add_media_context,
          &data)) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR, SDP_AGENT_UNEXPECTED_ERROR,
        "can not create SDP context");
    goto error;
  }

  return ctx;

error:
  kms_sdp_message_context_unref (ctx);
  return NULL;
}

GstSDPMessage *
kms_sdp_message_context_get_sdp_message (SdpMessageContext * ctx)
{
  return ctx->msg;
}

GSList *
kms_sdp_message_context_get_medias (SdpMessageContext * ctx)
{
  return ctx->medias;
}

SdpMediaConfig *
kms_sdp_message_context_get_media (SdpMessageContext * ctx, guint idx)
{
  return (SdpMediaConfig *) g_slist_nth_data (ctx->medias, idx);
}

void
kms_sdp_message_context_set_type (SdpMessageContext * ctx,
    KmsSdpMessageType type)
{
  ctx->type = type;
}

KmsSdpMessageType
kms_sdp_message_context_get_type (SdpMessageContext * ctx)
{
  return ctx->type;
}

gboolean
kms_sdp_message_context_set_origin (SdpMessageContext * ctx,
    const GstSDPOrigin * origin, GError ** error)
{
  if (gst_sdp_message_set_origin (ctx->msg, origin->username, origin->sess_id,
          origin->sess_version, origin->nettype, origin->addrtype,
          origin->addr) != GST_SDP_OK) {
    g_set_error_literal (error, KMS_SDP_AGENT_ERROR,
        SDP_AGENT_INVALID_PARAMETER, "Can not set attr: origin");
    return FALSE;
  }

  return TRUE;
}

KmsSdpMessageType kms_sdp_message_context_get_type (SdpMessageContext * ctx);

static void init_debug (void) __attribute__ ((constructor));

static void
init_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}
