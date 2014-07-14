#include <gst/gst.h>
#include "MediaType.hpp"
#include "MediaSourceImpl.hpp"
#include "MediaSinkImpl.hpp"
#include "MediaElementImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_element_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaElementImpl"

namespace kurento
{

MediaElementImpl::MediaElementImpl ()
{
  // FIXME: Implement this
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSrcs: Not implemented");
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs (std::shared_ptr<MediaType> mediaType)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSrcs: Not implemented");
}

std::vector<std::shared_ptr<MediaSource>> MediaElementImpl::getMediaSrcs (std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSrcs: Not implemented");
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSinks: Not implemented");
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks (std::shared_ptr<MediaType> mediaType)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSinks: Not implemented");
}

std::vector<std::shared_ptr<MediaSink>> MediaElementImpl::getMediaSinks (std::shared_ptr<MediaType> mediaType, const std::string &description)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::getMediaSinks: Not implemented");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::connect: Not implemented");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::connect: Not implemented");
}

void MediaElementImpl::connect (std::shared_ptr<MediaElement> sink, std::shared_ptr<MediaType> mediaType, const std::string &mediaDescription)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaElementImpl::connect: Not implemented");
}

MediaElementImpl::StaticConstructor MediaElementImpl::staticConstructor;

MediaElementImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
