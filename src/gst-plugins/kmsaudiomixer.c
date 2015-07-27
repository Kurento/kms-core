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
#include "config.h"
#endif

#include <gst/gst.h>

#include "kmsaudiomixer.h"
#include "kmsloop.h"
#include "kmsrefstruct.h"
#include "kmsagnosticbin.h"

#define PLUGIN_NAME "kmsaudiomixer"
#define KEY_SINK_PAD_NAME "kms-key-sink-pad-name"

#define KMS_LABEL_AUDIOMIXER "audiomixer"
#define KMS_LABEL_AGNOSTICBIN "agnosticbin"
#define KMS_LABEL_ADDER "adder"
#define LATENCY 250             //ms

#define KMS_AUDIO_MIXER_LOCK(mixer) \
  (g_rec_mutex_lock (&(mixer)->priv->mutex))

#define KMS_AUDIO_MIXER_UNLOCK(mixer) \
  (g_rec_mutex_unlock (&(mixer)->priv->mutex))

GST_DEBUG_CATEGORY_STATIC (kms_audio_mixer_debug_category);
#define GST_CAT_DEFAULT kms_audio_mixer_debug_category

#define KMS_AUDIO_MIXER_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (            \
    (obj),                                 \
    KMS_TYPE_AUDIO_MIXER,                  \
    KmsAudioMixerPrivate                   \
  )                                        \
)

#define KEY_TEE "tee_key"
#define KEY_FAKESINK "fakesink_key"
#define KEY_PAD "pad_key"

struct _KmsAudioMixerPrivate
{
  GRecMutex mutex;
  GHashTable *adders;
  GHashTable *agnostics;
  GHashTable *typefinds;
  KmsLoop *loop;
  guint count;
};

#define RAW_AUDIO_CAPS "audio/x-raw;"

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate audio_sink_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SINK_PAD,
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (RAW_AUDIO_CAPS)
    );

static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SRC_PAD,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (RAW_AUDIO_CAPS)
    );

static void unlink_agnosticbin (GstElement * agnosticbin);
static void unlink_adder_sources (GstElement * adder);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsAudioMixer, kms_audio_mixer,
    GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (kms_audio_mixer_debug_category,
        PLUGIN_NAME, 0, "debug category for " PLUGIN_NAME " element"));

static GstPadProbeReturn
cb_latency (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  if (GST_QUERY_TYPE (GST_PAD_PROBE_INFO_QUERY (info)) != GST_QUERY_LATENCY) {
    return GST_PAD_PROBE_OK;
  }

  GST_LOG_OBJECT (pad, "Modifing latency query. New latency %ld",
      LATENCY * GST_MSECOND);

  gst_query_set_latency (GST_PAD_PROBE_INFO_QUERY (info),
      TRUE, LATENCY * GST_MSECOND, LATENCY * GST_MSECOND);

  return GST_PAD_PROBE_OK;
}

static void
link_new_agnosticbin (gchar * key, GstElement * adder, GstElement * agnosticbin)
{
  GstPad *srcpad = NULL, *sinkpad = NULL;
  char *padname;

  padname = g_object_get_data (G_OBJECT (agnosticbin), KEY_SINK_PAD_NAME);
  if (padname == NULL) {
    GST_ERROR ("No pad associated with %" GST_PTR_FORMAT, agnosticbin);
    goto end;
  }

  if (g_str_equal (key, padname)) {
    /* Do not connect the origin audio input */
    GST_TRACE ("Do not connect echo audio input %" GST_PTR_FORMAT, agnosticbin);
    goto end;
  }

  srcpad = gst_element_get_request_pad (agnosticbin, "src_%u");
  if (srcpad == NULL) {
    GST_ERROR ("Could not get src pad in %" GST_PTR_FORMAT, agnosticbin);
    goto end;
  }

  sinkpad = gst_element_get_request_pad (adder, "sink_%u");
  if (srcpad == NULL) {
    GST_ERROR ("Could not get sink pad in %" GST_PTR_FORMAT, adder);
    gst_element_release_request_pad (agnosticbin, srcpad);
    goto end;
  }

  GST_DEBUG ("Linking %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, srcpad,
      sinkpad);

  gst_pad_add_probe (sinkpad,
      GST_PAD_PROBE_TYPE_QUERY_UPSTREAM,
      (GstPadProbeCallback) cb_latency, NULL, NULL);

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    GST_ERROR ("Could not link %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, srcpad,
        sinkpad);
    gst_element_release_request_pad (agnosticbin, srcpad);
    gst_element_release_request_pad (adder, sinkpad);
  }

end:
  if (srcpad != NULL) {
    g_object_unref (srcpad);
  }

  if (sinkpad != NULL) {
    g_object_unref (sinkpad);
  }
}

static void
link_new_adder (gchar * key, GstElement * agnosticbin, GstElement * adder)
{
  char *padname;
  GstPad *srcpad = NULL, *sinkpad = NULL;

  padname = g_object_get_data (G_OBJECT (adder), KEY_SINK_PAD_NAME);
  if (padname == NULL) {
    GST_ERROR ("No pad associated with %" GST_PTR_FORMAT, adder);
    return;
  }

  if (g_str_equal (key, padname)) {
    /* Do not connect the origin audio input */
    GST_TRACE ("Do not connect echo audio input %" GST_PTR_FORMAT, agnosticbin);
    return;
  }

  GST_DEBUG ("Linking %s to %s", GST_ELEMENT_NAME (agnosticbin),
      GST_ELEMENT_NAME (adder));

  srcpad = gst_element_get_request_pad (agnosticbin, "src_%u");
  if (srcpad == NULL) {
    GST_ERROR ("Could not get src pad in %" GST_PTR_FORMAT, agnosticbin);
    goto end;
  }

  sinkpad = gst_element_get_request_pad (adder, "sink_%u");
  if (srcpad == NULL) {
    GST_ERROR ("Could not get sink pad in %" GST_PTR_FORMAT, adder);
    gst_element_release_request_pad (agnosticbin, srcpad);
    goto end;
  }

  gst_pad_add_probe (sinkpad,
      GST_PAD_PROBE_TYPE_QUERY_UPSTREAM,
      (GstPadProbeCallback) cb_latency, NULL, NULL);

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    GST_ERROR ("Could not link %s to %s", GST_ELEMENT_NAME (agnosticbin),
        GST_ELEMENT_NAME (adder));

end:
  if (srcpad != NULL) {
    g_object_unref (srcpad);
  }

  if (sinkpad != NULL) {
    g_object_unref (sinkpad);
  }
}

static gint
get_stream_id_from_padname (const gchar * name)
{
  gint64 id;

  if (name == NULL)
    return -1;

  if (!g_str_has_prefix (name, AUDIO_SINK_PAD_PREFIX))
    return -1;

  id = g_ascii_strtoll (name + LENGTH_AUDIO_SINK_PAD_PREFIX, NULL, 10);
  if (id > G_MAXINT)
    return -1;

  return id;
}

static void
kms_audio_mixer_remove_sometimes_src_pad (KmsAudioMixer * self,
    GstElement * adder)
{
  GstPad *pad, *peer = NULL, *adder_src;

  pad = g_object_get_data (G_OBJECT (adder), KEY_PAD);
  g_object_set_data (G_OBJECT (adder), KEY_PAD, NULL);

  if (!pad) {
    return;
  }

  adder_src = gst_element_get_static_pad (adder, "src");
  if (adder_src) {
    peer = gst_pad_get_peer (adder_src);

    if (peer) {
      gst_pad_send_event (peer, gst_event_new_flush_start ());
    }
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD (pad), NULL);

  if (GST_STATE (self) < GST_STATE_PAUSED
      || GST_STATE_PENDING (self) < GST_STATE_PAUSED
      || GST_STATE_TARGET (self) < GST_STATE_PAUSED) {
    gst_pad_set_active (GST_PAD (pad), FALSE);
  }

  GST_DEBUG ("Removing source pad %" GST_PTR_FORMAT, pad);

  gst_element_remove_pad (GST_ELEMENT (self), GST_PAD (pad));

  if (peer) {
    gst_pad_send_event (peer, gst_event_new_flush_stop (FALSE));
    g_object_unref (peer);
  }

  if (adder_src) {
    g_object_unref (adder_src);
  }
}

static void
remove_element (GstBin * bin, GstElement * element)
{
  gst_object_ref (element);
  gst_element_set_locked_state (element, TRUE);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (bin, element);
  gst_object_unref (element);
}

static gboolean
remove_adder (GstElement * adder)
{
  KmsAudioMixer *self;
  GstElement *fakesink, *tee;

  self = (KmsAudioMixer *) gst_element_get_parent (adder);
  if (self == NULL) {
    GST_WARNING_OBJECT (adder, "No parent element");
    return FALSE;
  }

  GST_DEBUG ("Removing element %" GST_PTR_FORMAT, adder);

  kms_audio_mixer_remove_sometimes_src_pad (self, adder);

  tee = g_object_get_data (G_OBJECT (adder), KEY_TEE);
  fakesink = g_object_get_data (G_OBJECT (adder), KEY_FAKESINK);

  remove_element (GST_BIN (self), adder);

  if (tee) {
    remove_element (GST_BIN (self), tee);
  }

  if (fakesink) {
    remove_element (GST_BIN (self), fakesink);
  }

  gst_object_unref (self);

  return G_SOURCE_REMOVE;
}

static void
remove_agnostic_bin (GstElement * agnosticbin)
{
  KmsAudioMixer *self;
  GstElement *audiorate = NULL, *typefind = NULL;
  GstPad *sinkpad, *peerpad;

  self = (KmsAudioMixer *) gst_element_get_parent (agnosticbin);

  if (self == NULL) {
    GST_WARNING_OBJECT (agnosticbin, "No parent element");
    return;
  }

  sinkpad = gst_element_get_static_pad (agnosticbin, "sink");
  peerpad = gst_pad_get_peer (sinkpad);
  if (peerpad == NULL) {
    GST_WARNING_OBJECT (sinkpad, "Not linked");
    gst_object_unref (sinkpad);
    goto end;
  }

  audiorate = gst_pad_get_parent_element (peerpad);
  gst_object_unref (sinkpad);
  gst_object_unref (peerpad);

  if (audiorate == NULL) {
    GST_WARNING_OBJECT (self, "No audiorate");
    goto end;
  }

  sinkpad = gst_element_get_static_pad (audiorate, "sink");
  peerpad = gst_pad_get_peer (sinkpad);
  if (peerpad == NULL) {
    GST_WARNING_OBJECT (sinkpad, "Not linked");
    gst_object_unref (sinkpad);
    goto end;
  }

  typefind = gst_pad_get_parent_element (peerpad);
  gst_object_unref (sinkpad);
  gst_object_unref (peerpad);

  if (typefind == NULL) {
    GST_WARNING_OBJECT (self, "No typefind");
    goto end;
  }

  gst_element_unlink_many (typefind, audiorate, agnosticbin, NULL);

  gst_element_set_locked_state (typefind, TRUE);
  gst_element_set_locked_state (audiorate, TRUE);
  gst_element_set_locked_state (agnosticbin, TRUE);

  gst_element_set_state (typefind, GST_STATE_NULL);
  gst_element_set_state (audiorate, GST_STATE_NULL);
  gst_element_set_state (agnosticbin, GST_STATE_NULL);

  gst_object_ref (agnosticbin);

  gst_bin_remove_many (GST_BIN (self), typefind, audiorate, agnosticbin, NULL);

  gst_object_unref (agnosticbin);

end:
  if (audiorate != NULL) {
    gst_object_unref (audiorate);
  }

  if (typefind != NULL) {
    gst_object_unref (typefind);
  }

  gst_object_unref (self);
}

static gboolean
remove_adder_cb (gpointer key, gpointer value, gpointer user_data)
{
  GstElement *adder = GST_ELEMENT (value);

  unlink_adder_sources (adder);
  remove_adder (adder);

  return TRUE;
}

static gboolean
remove_agnosticbin_cb (gpointer key, gpointer value, gpointer user_data)
{
  GstElement *agnostic = GST_ELEMENT (value);

  unlink_agnosticbin (agnostic);
  remove_agnostic_bin (agnostic);

  return TRUE;
}

static void
kms_audio_mixer_dispose (GObject * object)
{
  KmsAudioMixer *self = KMS_AUDIO_MIXER (object);

  GST_DEBUG_OBJECT (self, "dispose");

  KMS_AUDIO_MIXER_LOCK (self);

  if (self->priv->agnostics != NULL) {
    g_hash_table_foreach_remove (self->priv->agnostics, remove_agnosticbin_cb,
        NULL);
    g_hash_table_unref (self->priv->agnostics);
    self->priv->agnostics = NULL;
  }

  if (self->priv->adders != NULL) {
    g_hash_table_foreach_remove (self->priv->adders, remove_adder_cb, self);
    g_hash_table_unref (self->priv->adders);
    self->priv->adders = NULL;
  }

  g_clear_object (&self->priv->loop);

  KMS_AUDIO_MIXER_UNLOCK (self);

  G_OBJECT_CLASS (kms_audio_mixer_parent_class)->dispose (object);
}

static void
kms_audio_mixer_finalize (GObject * object)
{
  KmsAudioMixer *self = KMS_AUDIO_MIXER (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_hash_table_unref (self->priv->typefinds);
  g_rec_mutex_clear (&self->priv->mutex);

  G_OBJECT_CLASS (kms_audio_mixer_parent_class)->finalize (object);
}

static void
kms_audio_mixer_have_type (GstElement * typefind, guint arg0, GstCaps * caps,
    gpointer data)
{
  KmsAudioMixer *self = KMS_AUDIO_MIXER (data);
  GstElement *audiorate, *agnosticbin;
  gchar *padname;
  gint id;

  padname = g_object_get_data (G_OBJECT (typefind), KEY_SINK_PAD_NAME);
  if ((id = get_stream_id_from_padname (padname)) < 0) {
    GST_ERROR_OBJECT (self, "Can not get pad id from element %" GST_PTR_FORMAT,
        typefind);
    return;
  }

  KMS_AUDIO_MIXER_LOCK (self);

  if (!g_hash_table_remove (self->priv->typefinds, padname)) {
    GST_WARNING_OBJECT (self, "Audio input %s is already managed", padname);
    KMS_AUDIO_MIXER_UNLOCK (self);
    return;
  }

  audiorate = gst_element_factory_make ("audiorate", NULL);
  agnosticbin = gst_element_factory_make ("agnosticbin", NULL);
  g_object_set_data_full (G_OBJECT (agnosticbin), KEY_SINK_PAD_NAME,
      g_strdup (padname), g_free);

  gst_bin_add_many (GST_BIN (self), audiorate, agnosticbin, NULL);
  gst_element_link_many (typefind, audiorate, agnosticbin, NULL);

  g_hash_table_foreach (self->priv->adders, (GHFunc) link_new_agnosticbin,
      agnosticbin);

  g_hash_table_insert (self->priv->agnostics, g_strdup (padname), agnosticbin);

  gst_bin_recalculate_latency (GST_BIN (self));
  KMS_AUDIO_MIXER_UNLOCK (self);

  gst_element_sync_state_with_parent (audiorate);
  gst_element_sync_state_with_parent (agnosticbin);
}

static void
unlink_agnosticbin_source (const GValue * item, gpointer user_data)
{
  GstElement *agnosticbin = GST_ELEMENT (user_data);
  GstPad *srcpad, *sinkpad = NULL;
  GstElement *adder = NULL;

  srcpad = g_value_get_object (item);

  sinkpad = gst_pad_get_peer (srcpad);
  if (sinkpad == NULL) {
    GST_WARNING_OBJECT (srcpad, "Not linked");
    goto end;
  }

  adder = gst_pad_get_parent_element (sinkpad);
  if (adder == NULL) {
    GST_ERROR_OBJECT (sinkpad, "No parent element");
    goto end;
  }

  GST_DEBUG ("Unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
      srcpad, sinkpad);

  if (!gst_pad_unlink (srcpad, sinkpad)) {
    GST_ERROR ("Can not unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
        srcpad, sinkpad);
  }

  gst_element_release_request_pad (adder, sinkpad);
  gst_element_release_request_pad (agnosticbin, srcpad);

end:
  if (sinkpad != NULL) {
    gst_object_unref (sinkpad);
  }

  if (adder != NULL) {
    gst_object_unref (adder);
  }
}

static void
unlink_agnosticbin (GstElement * agnosticbin)
{
  GstIterator *it;

  it = gst_element_iterate_src_pads (agnosticbin);

  while (gst_iterator_foreach (it, unlink_agnosticbin_source,
          agnosticbin) == GST_ITERATOR_RESYNC) {
    gst_iterator_resync (it);
  }

  gst_iterator_free (it);
}

static void
kms_audio_mixer_remove_elements (KmsAudioMixer * self,
    GstElement * agnosticbin, GstElement * adder)
{
  /* Unlink elements holding the mutex to avoid race */
  /* condition under massive disconnections */
  KMS_AUDIO_MIXER_LOCK (self);

  if (agnosticbin != NULL) {
    unlink_agnosticbin (agnosticbin);
  }

  if (adder != NULL) {
    unlink_adder_sources (adder);
  }

  KMS_AUDIO_MIXER_UNLOCK (self);

  if (agnosticbin != NULL) {
    remove_agnostic_bin (agnosticbin);
  }

  if (adder != NULL) {
    remove_adder (adder);
  }
}

static void
unlink_adder_sink (const GValue * item, gpointer user_data)
{
  GstElement *adder = GST_ELEMENT (user_data);
  GstPad *sinkpad, *srcpad = NULL;
  GstElement *src = NULL;

  sinkpad = g_value_get_object (item);

  srcpad = gst_pad_get_peer (sinkpad);
  if (srcpad == NULL) {
    GST_WARNING_OBJECT (sinkpad, "Not linked");
    goto end;
  }

  src = gst_pad_get_parent_element (srcpad);
  if (src == NULL) {
    GST_ERROR_OBJECT (srcpad, "No parent element");
    goto end;
  }

  GST_DEBUG ("Unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
      srcpad, sinkpad);

  if (!gst_pad_unlink (srcpad, sinkpad)) {
    GST_ERROR ("Can not unlink %" GST_PTR_FORMAT " and %" GST_PTR_FORMAT,
        srcpad, sinkpad);
  }

  gst_element_release_request_pad (adder, sinkpad);

  if (KMS_IS_AGNOSTIC_BIN2 (src)) {
    gst_element_release_request_pad (src, srcpad);
  }

end:
  g_clear_object (&srcpad);
  g_clear_object (&src);
}

static void
unlink_adder_sources (GstElement * adder)
{
  GstIterator *it;

  it = gst_element_iterate_sink_pads (adder);

  while (gst_iterator_foreach (it, unlink_adder_sink,
          adder) == GST_ITERATOR_RESYNC) {
    gst_iterator_resync (it);
  }

  gst_iterator_free (it);
}

static void
unlinked_pad (GstPad * pad, GstPad * peer, gpointer user_data)
{
  GstElement *agnostic = NULL, *adder = NULL, *typefind = NULL, *parent;
  KmsAudioMixer *self;
  gchar *padname;

  GST_DEBUG ("Unlinked pad %" GST_PTR_FORMAT, pad);
  parent = gst_pad_get_parent_element (pad);

  if (parent == NULL)
    return;

  self = KMS_AUDIO_MIXER (parent);

  if (gst_pad_get_direction (pad) != GST_PAD_SINK)
    goto end;

  padname = gst_pad_get_name (pad);

  KMS_AUDIO_MIXER_LOCK (self);

  typefind = g_hash_table_lookup (self->priv->typefinds, padname);
  if (typefind != NULL) {
    g_hash_table_remove (self->priv->typefinds, padname);
  }

  if (self->priv->agnostics != NULL) {
    agnostic = g_hash_table_lookup (self->priv->agnostics, padname);
    g_hash_table_remove (self->priv->agnostics, padname);
  }

  if (self->priv->adders != NULL) {
    adder = g_hash_table_lookup (self->priv->adders, padname);
    g_hash_table_remove (self->priv->adders, padname);
  }

  KMS_AUDIO_MIXER_UNLOCK (self);

  g_free (padname);

  if (GST_STATE (parent) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (parent) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (parent) >= GST_STATE_PAUSED) {
    if (typefind != NULL) {
      GST_WARNING_OBJECT (pad, "Removed before connecting branch");
      kms_audio_mixer_remove_elements (self, agnostic, adder);
      gst_object_ref (typefind);
      gst_element_set_locked_state (typefind, TRUE);
      gst_element_set_state (typefind, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self), typefind);
      gst_object_unref (typefind);
    } else {
      kms_audio_mixer_remove_elements (self, agnostic, adder);
    }
  } else {
    kms_audio_mixer_remove_elements (self, agnostic, adder);
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD (pad), NULL);

end:
  gst_object_unref (parent);
}

static gboolean
kms_audio_mixer_add_src_pad (KmsAudioMixer * self, const char *padname)
{
  GstPad *srcpad = NULL, *pad, *sinkpad = NULL;
  GstElement *adder;
  GstElement *audiotestsrc;
  GstElement *tee, *fakesink;
  gchar *srcname;
  gint id;

  if ((id = get_stream_id_from_padname (padname)) < 0) {
    GST_ERROR_OBJECT (self, "Can not get pad id from element %s", padname);
    return FALSE;
  }

  adder = gst_element_factory_make ("audiomixer", NULL);
  tee = gst_element_factory_make ("tee", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (tee, "allow-not-linked", TRUE, NULL);
  g_object_set (fakesink, "sync", FALSE, "async", FALSE, NULL);

  g_object_set_data_full (G_OBJECT (adder), KEY_SINK_PAD_NAME,
      g_strdup (padname), g_free);
  audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (audiotestsrc, "is-live", TRUE, "wave", /*silence */ 4, NULL);

  gst_bin_add_many (GST_BIN (self), audiotestsrc, adder, tee, fakesink, NULL);

  srcpad = gst_element_get_static_pad (audiotestsrc, "src");
  if (srcpad == NULL) {
    GST_ERROR ("Could not get src pad in %" GST_PTR_FORMAT, audiotestsrc);
    goto no_audiotestsrc;
  }

  sinkpad = gst_element_get_request_pad (adder, "sink_%u");
  if (sinkpad == NULL) {
    GST_ERROR ("Could not get sink pad in %" GST_PTR_FORMAT, adder);
    goto no_audiotestsrc;
  }

  GST_DEBUG ("Linking %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, srcpad,
      sinkpad);

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    GST_ERROR ("Could not link %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, srcpad,
        sinkpad);
    gst_element_release_request_pad (adder, sinkpad);
  }

  gst_element_link_many (adder, tee, fakesink, NULL);

no_audiotestsrc:
  if (srcpad != NULL) {
    g_object_unref (srcpad);
  }
  if (sinkpad != NULL) {
    g_object_unref (sinkpad);
  }

  gst_element_sync_state_with_parent (adder);
  gst_element_sync_state_with_parent (audiotestsrc);
  gst_element_sync_state_with_parent (fakesink);
  gst_element_sync_state_with_parent (tee);

  KMS_AUDIO_MIXER_LOCK (self);

  g_hash_table_foreach (self->priv->agnostics, (GHFunc) link_new_adder, adder);
  g_hash_table_insert (self->priv->adders, g_strdup (padname), adder);

  srcname = g_strdup_printf ("src_%u", id);
  srcpad = gst_element_get_request_pad (tee, "src_%u");
  pad = gst_ghost_pad_new (srcname, srcpad);
  g_free (srcname);
  gst_object_unref (srcpad);

  g_object_set_data (G_OBJECT (adder), KEY_TEE, tee);
  g_object_set_data (G_OBJECT (adder), KEY_FAKESINK, fakesink);
  g_object_set_data (G_OBJECT (adder), KEY_PAD, pad);

  if (GST_STATE (self) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (self) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (self) >= GST_STATE_PAUSED)
    gst_pad_set_active (pad, TRUE);

  if (gst_element_add_pad (GST_ELEMENT (self), pad)) {
    KMS_AUDIO_MIXER_UNLOCK (self);
    gst_bin_recalculate_latency (GST_BIN (self));
    return TRUE;
  }

  /* ERROR */
  GST_ERROR_OBJECT (self, "Can not add pad %" GST_PTR_FORMAT, pad);
  g_hash_table_remove (self->priv->adders, padname);

  KMS_AUDIO_MIXER_UNLOCK (self);

  unlink_adder_sources (adder);

  gst_object_unref (pad);

  gst_element_set_locked_state (adder, TRUE);
  gst_element_set_state (adder, GST_STATE_NULL);

  gst_bin_remove (GST_BIN (self), adder);

  return FALSE;
}

static GstPad *
kms_audio_mixer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  KmsAudioMixer *self = KMS_AUDIO_MIXER (element);
  GstPad *sinkpad, *pad = NULL;
  GstElement *typefind;
  gchar *padname;

  if (templ !=
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (G_OBJECT_GET_CLASS
              (element)), AUDIO_SINK_PAD))
    return NULL;

  typefind = gst_element_factory_make ("typefind", NULL);
  sinkpad = gst_element_get_static_pad (typefind, "sink");
  if (sinkpad == NULL) {
    gst_object_unref (typefind);
    return NULL;
  }

  gst_bin_add (GST_BIN (self), typefind);
  gst_element_sync_state_with_parent (typefind);

  KMS_AUDIO_MIXER_LOCK (self);

  padname = g_strdup_printf (AUDIO_SINK_PAD, self->priv->count++);

  pad = gst_ghost_pad_new (padname, sinkpad);
  g_object_unref (sinkpad);

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (element) >= GST_STATE_PAUSED)
    gst_pad_set_active (pad, TRUE);

  if (!gst_element_add_pad (element, pad)) {
    GST_ERROR_OBJECT (self, "Could not create sink pad");
  } else if (!kms_audio_mixer_add_src_pad (self, padname)) {
    GST_ERROR_OBJECT (self, "Could not create source pad");
    if (gst_pad_is_active (pad)) {
      gst_pad_set_active (pad, FALSE);
    }
    gst_element_remove_pad (element, pad);
  } else {
    g_hash_table_insert (self->priv->typefinds, g_strdup (padname), typefind);
    g_object_set_data_full (G_OBJECT (typefind), KEY_SINK_PAD_NAME, padname,
        g_free);
    g_signal_connect (G_OBJECT (typefind), "have-type",
        G_CALLBACK (kms_audio_mixer_have_type), self);
    goto end;
  }

  /* Error */
  g_object_unref (pad);
  gst_element_set_locked_state (typefind, TRUE);
  gst_element_set_state (typefind, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self), typefind);
  self->priv->count--;
  pad = NULL;
  g_free (padname);

end:
  KMS_AUDIO_MIXER_UNLOCK (self);

  if (pad != NULL) {
    g_signal_connect (G_OBJECT (pad), "unlinked",
        G_CALLBACK (unlinked_pad), NULL);
  }

  return pad;
}

static void
kms_audio_mixer_release_pad (GstElement * element, GstPad * pad)
{
  GST_DEBUG ("Release pad %" GST_PTR_FORMAT, pad);

  if (gst_pad_get_direction (pad) != GST_PAD_SINK)
    return;

  if (GST_STATE (element) >= GST_STATE_PAUSED
      || GST_STATE_PENDING (element) >= GST_STATE_PAUSED
      || GST_STATE_TARGET (element) >= GST_STATE_PAUSED) {
    gst_pad_set_active (pad, FALSE);
  }

  gst_element_remove_pad (element, pad);
}

static void
kms_audio_mixer_class_init (KmsAudioMixerClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_static_metadata (gstelement_class,
      "AudioMixer", "Generic",
      "Audio mixer element",
      "Santiago Carot Nemesio <sancane at gmail dot com>");

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (kms_audio_mixer_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (kms_audio_mixer_release_pad);
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_src_factory));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (kms_audio_mixer_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (kms_audio_mixer_finalize);

  /* Registers a private structure for the instantiatable type */
  g_type_class_add_private (klass, sizeof (KmsAudioMixerPrivate));
}

static void
kms_audio_mixer_init (KmsAudioMixer * self)
{
  self->priv = KMS_AUDIO_MIXER_GET_PRIVATE (self);

  self->priv->adders = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      NULL);
  self->priv->agnostics =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->priv->typefinds =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_rec_mutex_init (&self->priv->mutex);
  self->priv->loop = kms_loop_new ();
}

gboolean
kms_audio_mixer_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_AUDIO_MIXER);
}
