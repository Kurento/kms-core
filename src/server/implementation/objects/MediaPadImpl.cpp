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

MediaPadImpl::MediaPadImpl (const boost::property_tree::ptree &config,
                            std::shared_ptr<MediaObjectImpl> parent,
                            std::shared_ptr<MediaType> mediaType,
                            const std::string &mediaDescription) : MediaObjectImpl (config, parent)
{
  this->mediaElement = std::dynamic_pointer_cast<MediaElement> (parent);
  this->mediaType = mediaType;
  this->mediaDescription = mediaDescription;
}

GstElement *
MediaPadImpl::getGstreamerElement ()
{
  std::shared_ptr<MediaElementImpl> element = std::dynamic_pointer_cast
      <MediaElementImpl> (getParent() );

  return element->getGstreamerElement();
}

MediaPadImpl::StaticConstructor MediaPadImpl::staticConstructor;

MediaPadImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
