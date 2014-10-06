#include <gst/gst.h>
#include "MediaPipelineImpl.hpp"
#include "MediaObjectImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#define GST_CAT_DEFAULT kurento_media_object_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaObjectImpl"

namespace kurento
{

MediaObjectImpl::MediaObjectImpl (const boost::property_tree::ptree &config)
{
  id = createId();
  this->config = config;
}

MediaObjectImpl::MediaObjectImpl (const boost::property_tree::ptree &config, std::shared_ptr< MediaObject > parent)
{
  this->parent = parent;
  id = createId();
  this->config = config;
}

std::shared_ptr<MediaPipeline>
MediaObjectImpl::getMediaPipeline ()
{
  if (parent) {
    return std::dynamic_pointer_cast<MediaObjectImpl> (parent)->getMediaPipeline();
  } else {
    return std::dynamic_pointer_cast<MediaPipeline> (shared_from_this() );
  }
}

std::string
MediaObjectImpl::createId()
{
  std::stringstream ss;
  boost::uuids::uuid uuid = boost::uuids::random_generator() ();

  ss << uuid;

  if (parent) {
    std::shared_ptr<MediaPipelineImpl> pipeline;

    pipeline = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );
    return pipeline->getId() + "/" +
           ss.str();
  } else {
    return ss.str();
  }
}

MediaObjectImpl::StaticConstructor MediaObjectImpl::staticConstructor;

MediaObjectImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
