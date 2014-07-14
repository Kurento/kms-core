#include <gst/gst.h>
#include "MediaSinkImpl.hpp"
#include "MediaSourceImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_source_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaSourceImpl"

namespace kurento
{

MediaSourceImpl::MediaSourceImpl ()
{
  // FIXME: Implement this
}

std::vector<std::shared_ptr<MediaSink>> MediaSourceImpl::getConnectedSinks ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaSourceImpl::getConnectedSinks: Not implemented");
}

void MediaSourceImpl::connect (std::shared_ptr<MediaSink> sink)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaSourceImpl::connect: Not implemented");
}

MediaSourceImpl::StaticConstructor MediaSourceImpl::staticConstructor;

MediaSourceImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
