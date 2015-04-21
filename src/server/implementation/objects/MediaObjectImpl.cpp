#include <gst/gst.h>
#include "MediaPipelineImpl.hpp"
#include "MediaObjectImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <UUIDGenerator.hpp>
#include <MediaSet.hpp>

#define GST_CAT_DEFAULT kurento_media_object_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaObjectImpl"

namespace kurento
{

MediaObjectImpl::MediaObjectImpl (const boost::property_tree::ptree &config) :
  MediaObjectImpl (config, std::shared_ptr<MediaObject> () )
{
}

MediaObjectImpl::MediaObjectImpl (const boost::property_tree::ptree &config,
                                  std::shared_ptr< MediaObject > parent)
{
  this->parent = parent;

  creationTime = time (NULL);
  initialId = createId();
  this->config = config;
  this->sendTagsInEvents = false;
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
  std::string uuid = generateUUID();

  if (parent) {
    std::shared_ptr<MediaPipelineImpl> pipeline;

    pipeline = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );
    return pipeline->getId() + "/" +
           uuid;
  } else {
    return uuid;
  }
}

std::string
MediaObjectImpl::getName()
{
  std::unique_lock<std::recursive_mutex> lck (mutex);

  if (name.empty () ) {
    name = getId ();
  }

  return name;
}

std::string
MediaObjectImpl::getId()
{
  std::unique_lock<std::recursive_mutex> lck (mutex);

  if (id.empty () ) {
    id = this->initialId + "_" + this->getModule() + "." + this->getType ();
  }

  return id;
}

void
MediaObjectImpl::setName (const std::string &name)
{
  std::unique_lock<std::recursive_mutex> lck (mutex);

  this->name = name;
}

std::vector<std::shared_ptr<MediaObject>> MediaObjectImpl::getChilds ()
{
  std::vector<std::shared_ptr<MediaObject>> childs;

  for (auto it : MediaSet::getMediaSet ()->getChilds (std::dynamic_pointer_cast
       <MediaObjectImpl> (shared_from_this() ) ) ) {
    childs.push_back (it);
  }

  return childs;
}

bool MediaObjectImpl::getSendTagsInEvents ()
{
  return this->sendTagsInEvents;
}

void MediaObjectImpl::setSendTagsInEvents (bool sendTagsInEvents)
{
  this->sendTagsInEvents = sendTagsInEvents;
}

void MediaObjectImpl::addTag (const std::string &key, const std::string &value)
{
  tagsMap [key] = value;
  GST_DEBUG ("Tag added");
}

void MediaObjectImpl::removeTag (const std::string &key)
{
  auto it = tagsMap.find (key);

  if (it != tagsMap.end() ) {
    tagsMap.erase (it);
    GST_DEBUG ("Tag deleted");
    return;
  }

  GST_DEBUG ("Tag not found");
}

std::string MediaObjectImpl::getTag (const std::string &key)
{
  auto it = tagsMap.find (key);

  if (it != tagsMap.end() ) {
    return it->second;
  }

  throw KurentoException (MEDIA_OBJECT_TAG_KEY_NOT_FOUND,
                          "Tag key not found");
}

void MediaObjectImpl::postConstructor()
{
}

std::vector<std::shared_ptr<Tag>> MediaObjectImpl::getTags ()
{
  std::vector<std::shared_ptr<Tag>> ret;

  for (auto it : tagsMap ) {
    std::shared_ptr <Tag> tag (new Tag (it.first, it.second) );
    ret.push_back (tag);
  }

  return ret;
}

int  MediaObjectImpl::getCreationTime ()
{
  return (int) creationTime;
}

MediaObjectImpl::StaticConstructor MediaObjectImpl::staticConstructor;

MediaObjectImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
