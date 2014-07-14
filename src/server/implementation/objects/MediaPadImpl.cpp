#include <gst/gst.h>
#include "MediaElementImpl.hpp"
#include "MediaType.hpp"
#include "MediaPadImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_media_pad_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaPadImpl"

namespace kurento
{

MediaPadImpl::MediaPadImpl ()
{
  // FIXME: Implement this
}

std::shared_ptr<MediaElement> MediaPadImpl::getMediaElement ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaPadImpl::getMediaElement: Not implemented");
}

std::shared_ptr<MediaType> MediaPadImpl::getMediaType ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaPadImpl::getMediaType: Not implemented");
}

std::string MediaPadImpl::getMediaDescription ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "MediaPadImpl::getMediaDescription: Not implemented");
}

MediaPadImpl::StaticConstructor MediaPadImpl::staticConstructor;

MediaPadImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
