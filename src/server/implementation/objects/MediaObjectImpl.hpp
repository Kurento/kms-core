#ifndef __MEDIA_OBJECT_IMPL_HPP__
#define __MEDIA_OBJECT_IMPL_HPP__

#include <Factory.hpp>
#include "MediaObject.hpp"
#include <EventHandler.hpp>
#include <boost/property_tree/ptree.hpp>
#include <mutex>
#include <map>

namespace kurento
{

class MediaPipelineImpl;
class MediaObjectImpl;

void Serialize (std::shared_ptr<MediaObjectImpl> &object,
                JsonSerializer &serializer);

class MediaObjectImpl : public virtual MediaObject
{

public:

  MediaObjectImpl (const boost::property_tree::ptree &config);

  MediaObjectImpl (const boost::property_tree::ptree &config,
                   std::shared_ptr <MediaObject> parent);

  virtual ~MediaObjectImpl () {}

  void addTag (const std::string &key, const std::string &value);
  void removeTag (const std::string &key);

  virtual std::shared_ptr<MediaPipeline> getMediaPipeline ();

  virtual std::shared_ptr<MediaObject> getParent ()
  {
    return parent;
  }

  virtual std::vector<std::shared_ptr<MediaObject>> getChilds ();

  virtual std::string getId ();

  virtual std::string getName ();
  virtual void setName (const std::string &name);

  virtual bool getSendTagsInEvents ();
  virtual void setSendTagsInEvents (bool sendTagsInEvents);

  virtual void release ()
  {

  }

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  sigc::signal<void, Error> signalError;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:

  template <class T, class C>
  T getConfigValue (const std::string &key)
  {
    return config.get<T> ("modules." + dynamic_cast <C *> (this)->getModule() + "."
                          + dynamic_cast <C *> (this)->getType() + "." + key );
  }

  template <class T, class C>
  T getConfigValue (const std::string &key, T defaultValue)
  {
    return config.get<T> ("modules." + dynamic_cast <C *> (this)->getModule() + "."
                          + dynamic_cast <C *> (this)->getType() + "." + key, defaultValue);
  }

  boost::property_tree::ptree config;

private:

  std::string initialId;
  std::string id;
  std::string name;
  std::recursive_mutex mutex;
  std::shared_ptr<MediaObject> parent;

  std::string createId();

  std::map<std::string, std::string> tagsMap;
  bool sendTagsInEvents;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __MEDIA_OBJECT_IMPL_HPP__ */
