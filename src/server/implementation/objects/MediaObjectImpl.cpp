#include <gst/gst.h>
#include "MediaPipelineImpl.hpp"
#include "MediaObjectImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>

#define GST_CAT_DEFAULT kurento_media_object_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaObjectImpl"

namespace kurento
{

class RandomGeneratorBase
{
protected:
  RandomGeneratorBase () {
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<time_t> time = std::chrono::duration_cast<std::chrono::duration<time_t>> (now.time_since_epoch () );

    ran.seed (time.count() );
  }

  boost::mt19937 ran;
};

class RandomGenerator : RandomGeneratorBase
{
  boost::mt19937 ran;
  boost::uuids::basic_random_generator<boost::mt19937> gen;

public:
  RandomGenerator () : RandomGeneratorBase(), gen (&ran) {
  }

  std::string getUUID () {
    std::stringstream ss;
    boost::uuids::uuid uuid = gen ();

    ss << uuid;
    return ss.str();
  }
};

static RandomGenerator gen;

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
  std::string uuid = gen.getUUID ();

  if (parent) {
    std::shared_ptr<MediaPipelineImpl> pipeline;

    pipeline = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );
    return pipeline->getId() + "/" +
           uuid;
  } else {
    return uuid;
  }
}

MediaObjectImpl::StaticConstructor MediaObjectImpl::staticConstructor;

MediaObjectImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
