#include <gst/gst.h>
#include "MediaSourceImpl.hpp"
#include "MediaSinkImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_sink_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaSinkImpl"

namespace kurento
{

MediaSinkImpl::MediaSinkImpl (std::shared_ptr<MediaType> mediaType,
                              const std::string &mediaDescription,
                              std::shared_ptr<MediaObjectImpl> parent)
{
  // FIXME: Implement this
}

void MediaSinkImpl::disconnect (std::shared_ptr<MediaSource> src)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaSinkImpl::disconnect: Not implemented");
}

std::shared_ptr<MediaSource> MediaSinkImpl::getConnectedSrc ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaSinkImpl::getConnectedSrc: Not implemented");
}

MediaSinkImpl::StaticConstructor MediaSinkImpl::staticConstructor;

MediaSinkImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
