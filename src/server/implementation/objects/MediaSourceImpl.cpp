#include <gst/gst.h>
#include "MediaSinkImpl.hpp"
#include "MediaSourceImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaType.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_source_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaSourceImpl"

namespace kurento
{

struct tmp_data {
  GRecMutex mutex;
  std::weak_ptr<MediaSourceImpl> src;
  std::weak_ptr<MediaSinkImpl> sink;
  gulong handler;
  int ref;
};

static void
destroy_tmp_data (gpointer data)
{
  struct tmp_data *tmp = (struct tmp_data *) data;

  tmp->src.reset();
  tmp->sink.reset();
  g_rec_mutex_clear (&tmp->mutex);
  g_slice_free (struct tmp_data, tmp);
}

static void
tmp_data_unref (struct tmp_data *data)
{
  gboolean dispose;

  g_rec_mutex_lock (&data->mutex);

  if (G_UNLIKELY (data->ref <= 0) ) {
    GST_WARNING ("Refcount <= 0");
  }

  data->ref --;
  dispose = data->ref == 0;
  g_rec_mutex_unlock (&data->mutex);

  if (dispose) {
    destroy_tmp_data (data);
  }
}

static void
tmp_data_unref_closure (gpointer data, GClosure *closure)
{
  tmp_data_unref ( (struct tmp_data *) data);
}

static struct tmp_data *
tmp_data_ref (struct tmp_data *data)
{
  g_rec_mutex_lock (&data->mutex);
  data->ref ++;
  g_rec_mutex_unlock (&data->mutex);

  return data;
}

static struct tmp_data *
create_tmp_data (std::shared_ptr<MediaSourceImpl> src,
                 std::shared_ptr<MediaSinkImpl> sink)
{
  struct tmp_data *tmp;

  tmp = g_slice_new0 (struct tmp_data);

  g_rec_mutex_init (&tmp->mutex);

  tmp->src = std::weak_ptr<MediaSourceImpl> (src);
  tmp->sink = std::weak_ptr<MediaSinkImpl> (sink);
  tmp->handler = 0L;
  tmp->ref = 1;

  return tmp;
}

static void
pad_unlinked (GstPad *pad, GstPad *peer, gpointer data)
{
  GstElement *parent = gst_pad_get_parent_element (pad);

  if (parent != NULL) {
    gst_element_release_request_pad (parent, pad);
    g_object_unref (parent);
  }
}

gboolean
link_media_pads (std::shared_ptr<MediaSourceImpl> src,
                 std::shared_ptr<MediaSinkImpl> sink)
{
  std::unique_lock<std::recursive_mutex> lock (src->mutex);
  bool ret = FALSE;
  GstPad *pad;

  pad = gst_element_get_request_pad (src->getGstreamerElement(),
                                     src->getPadName() );

  if (pad == NULL) {
    return FALSE;
  }

  GST_DEBUG ("Connecting pad %s", src->getPadName() );

  g_signal_connect_data (G_OBJECT (pad), "unlinked", G_CALLBACK (pad_unlinked),
                         g_object_ref (src->getGstreamerElement() ),
                         (GClosureNotify) g_object_unref, (GConnectFlags) 0);

  if (sink->linkPad (src, pad) ) {
    src->connectedSinks.push_back (std::weak_ptr<MediaSinkImpl> (sink) );
    ret = TRUE;
  } else {
    gst_element_release_request_pad (src->getGstreamerElement(), pad);
    ret = FALSE;
  }

  gst_object_unref (pad);

  return ret;
}

static void
disconnect_handler (GstElement *element, struct tmp_data *data)
{
  if (data->handler != 0) {
    g_signal_handler_disconnect (element, data->handler);
    data->handler = 0;
  }
}

static void
link_media_elements (GstElement *element, gpointer data)
{
  struct tmp_data *tmp = (struct tmp_data *) data;
  std::shared_ptr<MediaSourceImpl> src;
  std::shared_ptr<MediaSinkImpl> sink;

  g_rec_mutex_lock (&tmp->mutex);

  if (tmp->handler == 0) {
    goto end;
  }

  try {
    src = tmp->src.lock();
    sink = tmp->sink.lock();

    if (src && sink && link_media_pads (src, sink) ) {
      disconnect_handler (element, tmp);
    }
  } catch (const std::bad_weak_ptr &e) {
    GST_WARNING ("Removed before connecting");

    disconnect_handler (element, tmp);
  }

end:
  g_rec_mutex_unlock (&tmp->mutex);
}

MediaSourceImpl::MediaSourceImpl (const boost::property_tree::ptree &config,
                                  std::shared_ptr<MediaType> mediaType,
                                  const std::string &mediaDescription,
                                  std::shared_ptr<MediaObjectImpl> parent) :
  MediaPadImpl (config, parent, mediaType, mediaDescription)
{
}

MediaSourceImpl::~MediaSourceImpl()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  for (auto it = connectedSinks.begin(); it != connectedSinks.end(); it++) {
    try {
      std::shared_ptr<MediaSinkImpl> connectedSinkLocked;

      GST_INFO ("connectedSink");
      connectedSinkLocked = it->lock();

      if (connectedSinkLocked != NULL) {
        connectedSinkLocked->unlinkUnchecked (NULL);
      }
    } catch (const std::bad_weak_ptr &e) {
      GST_WARNING ("Got invalid reference while releasing MediaSrc %s",
                   getId().c_str() );
    }
  }
}

const gchar *
MediaSourceImpl::getPadName ()
{
  if ( ( (MediaPadImpl *) this)->getMediaType()->getValue() == MediaType::AUDIO) {
    return (const gchar *) "audio_src_%u";
  } else {
    return (const gchar *) "video_src_%u";
  }
}

void MediaSourceImpl::connect (std::shared_ptr<MediaSink> sink)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<MediaSinkImpl> mediaSinkImpl =
    std::dynamic_pointer_cast<MediaSinkImpl> (sink);
  struct tmp_data *tmp;

  GST_INFO ("connect %s to %s", this->getId().c_str(),
            mediaSinkImpl->getId().c_str() );

  tmp = create_tmp_data (std::dynamic_pointer_cast<MediaSourceImpl>
                         (shared_from_this() ), mediaSinkImpl);
  tmp_data_ref (tmp);
  tmp->handler = g_signal_connect_data (getGstreamerElement(),
                                        "agnosticbin-added",
                                        G_CALLBACK (link_media_elements),
                                        tmp, tmp_data_unref_closure,
                                        (GConnectFlags) 0);

  link_media_elements (getGstreamerElement(), tmp);
  tmp_data_unref (tmp);
}

void
MediaSourceImpl::removeSink (MediaSinkImpl *mediaSink)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::shared_ptr<MediaSinkImpl> sinkLocked;
  std::vector< std::weak_ptr<MediaSinkImpl> >::iterator it;

  it = connectedSinks.begin();

  while (it != connectedSinks.end() ) {
    try {
      sinkLocked = (*it).lock();
    } catch (const std::bad_weak_ptr &e) {
    }

    if (sinkLocked == NULL || sinkLocked->getId() == mediaSink->getId() ) {
      it = connectedSinks.erase (it);
    } else {
      it++;
    }
  }
}

void
MediaSourceImpl::disconnect (MediaSinkImpl *mediaSink)
{
  std::unique_lock<std::recursive_mutex> lock (mutex);

  GST_INFO ("disconnect %s from %s", this->getId().c_str(),
            mediaSink->getId().c_str() );

  mediaSink->unlink (std::dynamic_pointer_cast<MediaSourceImpl>
                     (shared_from_this() ), NULL);
}

std::vector < std::shared_ptr<MediaSink> >
MediaSourceImpl::getConnectedSinks ()
{
  std::unique_lock<std::recursive_mutex> lock (mutex);
  std::vector < std::shared_ptr<MediaSink> > sinks;

  std::shared_ptr<MediaSinkImpl> sinkLocked;
  std::vector< std::weak_ptr<MediaSinkImpl> >::iterator it;

  for ( it = connectedSinks.begin() ; it != connectedSinks.end(); ++it) {
    try {
      sinkLocked = (*it).lock();
    } catch (const std::bad_weak_ptr &e) {
    }

    if (sinkLocked != NULL) {
      sinks.push_back (std::dynamic_pointer_cast<MediaSink> (sinkLocked) );
    }
  }

  return sinks;
}

MediaSourceImpl::StaticConstructor MediaSourceImpl::staticConstructor;

MediaSourceImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
