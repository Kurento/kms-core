#include <gst/gst.h>
#include "MediaSourceImpl.hpp"
#include "MediaSinkImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaType.hpp>
#include <gst/gst.h>
#include <condition_variable>

#define GST_CAT_DEFAULT kurento_media_sink_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaSinkImpl"

namespace kurento
{

MediaSinkImpl::MediaSinkImpl (std::shared_ptr<MediaType> mediaType,
                              const std::string &mediaDescription,
                              std::shared_ptr<MediaObjectImpl> parent) :
  MediaPadImpl (parent, mediaType, mediaDescription)
{
}

MediaSinkImpl::~MediaSinkImpl()
{
  std::shared_ptr<MediaSourceImpl> connectedSrcLocked;

  try {
    connectedSrcLocked = connectedSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (connectedSrcLocked != NULL) {
    disconnect (connectedSrcLocked);
  }
}

void MediaSinkImpl::disconnect (std::shared_ptr<MediaSource> src)
{
  std::shared_ptr<MediaSourceImpl> mediaSourceImpl;

  mediaSourceImpl = std::dynamic_pointer_cast<MediaSourceImpl> (src);

  mediaSourceImpl->disconnect (this);
}

std::shared_ptr<MediaSource> MediaSinkImpl::getConnectedSrc ()
{
  std::shared_ptr<MediaSourceImpl> connectedSrcLocked;

  try {
    connectedSrcLocked = connectedSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  return std::dynamic_pointer_cast<MediaSource> (connectedSrcLocked);
}

std::string
MediaSinkImpl::getPadName ()
{
  if (getMediaType()->getValue() == MediaType::AUDIO) {
    return "audio_sink";
  } else {
    return "video_sink";
  }
}

static void
remove_from_parent (GstElement *element)
{
  GstBin *parent = GST_BIN (GST_OBJECT_PARENT (element) );

  if (parent == NULL) {
    return;
  }

  gst_element_set_locked_state (element, TRUE);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (parent, element);
}

static void
sink_unlinked (GstPad *pad, GstPad *peer, GstElement *filter)
{
  GstPad *src;
  GstPad *src_peer;

  src = gst_element_get_static_pad (filter, "src");
  src_peer = gst_pad_get_peer (src);

  if (src_peer != NULL) {
    gst_pad_unlink (src, src_peer);
    gst_object_unref (src_peer);
  } else {
    remove_from_parent (filter);
  }

  gst_object_unref (src);
}

static void
src_unlinked (GstPad *pad, GstPad *peer, GstElement *filter)
{
  GstPad *sink;
  GstPad *sink_peer;

  sink = gst_element_get_static_pad (filter, "sink");
  sink_peer = gst_pad_get_peer (sink);

  if (sink_peer != NULL) {
    gst_pad_unlink (sink_peer, sink);
    gst_object_unref (sink_peer);
  } else {
    remove_from_parent (filter);
  }

  gst_object_unref (sink);
}

bool
MediaSinkImpl::linkPad (std::shared_ptr<MediaSourceImpl> mediaSrc, GstPad *src)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<MediaSourceImpl> connectedSrcLocked;
  GstPad *sink;
  bool ret = false;

  try {
    connectedSrcLocked = connectedSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if ( (sink = gst_element_get_static_pad (getGstreamerElement(),
               getPadName().c_str() ) ) == NULL) {
    sink = gst_element_get_request_pad (getGstreamerElement(),
                                        getPadName().c_str() );
  }

  if (gst_pad_is_linked (sink) ) {
    unlink (connectedSrcLocked, sink);
  }

  if (std::dynamic_pointer_cast<MediaObjectImpl> (mediaSrc)->getParent() ==
      getParent() ) {
    GstBin *container;
    GstElement *filter, *parent;
    GstPad *aux_sink, *aux_src;

    GST_DEBUG ("Connecting loopback, adding a capsfilter to allow connection");
    parent = GST_ELEMENT (GST_OBJECT_PARENT (sink) );

    if (parent == NULL) {
      goto end;
    }

    container = GST_BIN (GST_OBJECT_PARENT (parent) );

    if (container == NULL) {
      goto end;
    }

    filter = gst_element_factory_make ("capsfilter", NULL);

    aux_sink = gst_element_get_static_pad (filter, "sink");
    aux_src = gst_element_get_static_pad (filter, "src");

    g_signal_connect (G_OBJECT (aux_sink), "unlinked", G_CALLBACK (sink_unlinked),
                      filter );
    g_signal_connect (G_OBJECT (aux_src), "unlinked", G_CALLBACK (src_unlinked),
                      filter );

    gst_bin_add (container, filter);
    gst_element_sync_state_with_parent (filter);

    if (gst_pad_link_full (aux_src, sink,
                           GST_PAD_LINK_CHECK_NOTHING) == GST_PAD_LINK_OK) {
      if (gst_pad_link_full (src, aux_sink,
                             GST_PAD_LINK_CHECK_NOTHING) == GST_PAD_LINK_OK) {
        ret = true;
      } else {
        gst_pad_unlink (aux_src, sink);
      }

    }

    g_object_unref (aux_sink);
    g_object_unref (aux_src);

    gst_debug_bin_to_dot_file_with_ts (GST_BIN (container),
                                       GST_DEBUG_GRAPH_SHOW_ALL, "loopback");

  } else {
    if (gst_pad_link_full (src, sink,
                           GST_PAD_LINK_CHECK_NOTHING) == GST_PAD_LINK_OK) {
      ret = true;
    }
  }

  if (ret == true) {
    connectedSrc = std::weak_ptr<MediaSourceImpl> (mediaSrc);
  } else {
    gst_element_release_request_pad (getGstreamerElement(), sink);
  }

end:

  g_object_unref (sink);

  return ret;
}

void
MediaSinkImpl::unlink (std::shared_ptr<MediaSourceImpl> mediaSrc, GstPad *sink)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<MediaSourceImpl> connectedSrcLocked;

  try {
    connectedSrcLocked = connectedSrc.lock();
  } catch (const std::bad_weak_ptr &e) {
  }

  if (connectedSrcLocked != NULL && mediaSrc == connectedSrcLocked) {
    unlinkUnchecked (sink);
    connectedSrcLocked->removeSink (this);
  }
}

static GstPadProbeReturn
pad_blocked_adaptor (GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
  gboolean processed = FALSE;

  auto handler =
    reinterpret_cast < std::function < void (GstPad * pad,
        GstPadProbeInfo * info) > * > (data);

  GST_OBJECT_LOCK (pad);

  processed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pad), "processed") );

  if (processed) {
    GST_OBJECT_UNLOCK (pad);
    return GST_PAD_PROBE_REMOVE;
  }

  g_object_set_data (G_OBJECT (pad), "processed", GINT_TO_POINTER (TRUE) );
  GST_OBJECT_UNLOCK (pad);

  (*handler) (pad, info);

  /* Drop everithing so as to avoid broken pipe errors on an unlinked pad */
  return GST_PAD_PROBE_REMOVE;
}

void
MediaSinkImpl::unlinkUnchecked (GstPad *sink)
{
  GstPad *peer;
  GstPad *sinkPad;

  if (sink == NULL) {
    sinkPad = gst_element_get_static_pad (getGstreamerElement(),
                                          getPadName().c_str() );
  } else {
    sinkPad = sink;
  }

  if (sinkPad == NULL) {
    return;
  }

  peer = gst_pad_get_peer (sinkPad);

  if (peer != NULL) {
    std::condition_variable cond;
    std::mutex cmutex;

    bool blocked = false;

    std::function <void (GstPad *, GstPadProbeInfo *) >
    blockedLambda = [&] (GstPad * pad, GstPadProbeInfo * info) {
      std::unique_lock<std::mutex> lock (cmutex);

      GST_DEBUG ("Peer pad blocked %" GST_PTR_FORMAT, pad);

      if (blocked) {
        return;
      }

      gst_pad_unlink (pad, sinkPad);
      blocked = TRUE;

      cond.notify_all();
    };

    gst_pad_add_probe (peer, (GstPadProbeType) (GST_PAD_PROBE_TYPE_BLOCKING),
                       pad_blocked_adaptor, &blockedLambda, NULL);

    {
      std::unique_lock<std::mutex> lock (cmutex);

      cond.wait (lock, [&blocked] () -> bool {
        return blocked;
      });
      std::condition_variable_any any;
    }

    g_object_unref (peer);
  }

  if (sink == NULL) {
    gst_element_release_request_pad (getGstreamerElement(), sinkPad);
    g_object_unref (sinkPad);
  }
}

MediaSinkImpl::StaticConstructor MediaSinkImpl::staticConstructor;

MediaSinkImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
