#ifndef __MEDIA_OBJECT_IMPL_HPP__
#define __MEDIA_OBJECT_IMPL_HPP__

#include <Factory.hpp>
#include "MediaObject.hpp"
#include <EventHandler.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <mutex>
#include <map>
#include "Tag.hpp"
#include <gst/gst.h>

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
  std::string getTag (const std::string &key);
  std::vector<std::shared_ptr<Tag>> getTags ();

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

  virtual int getCreationTime ();

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
    auto child = config.get_child ("modules." + dynamic_cast <C *>
                                   (this)->getModule() + "."
                                   + dynamic_cast <C *> (this)->getType() + "." + key);
    std::stringstream ss;
    Json::Value val;
    Json::Reader reader;
    kurento::JsonSerializer serializer (false);
    boost::property_tree::ptree array;

    array.push_back (std::make_pair ("val", child) );
    boost::property_tree::write_json (ss, array);

    reader.parse (ss.str(), val);

    T ret {};

    serializer.JsonValue = val;
    serializer.Serialize ("val", ret);

    return ret;
  }

  template <class T, class C>
  T getConfigValue (const std::string &key, T defaultValue)
  {
    try {
      return getConfigValue<T, C> (key);
    } catch (boost::property_tree::ptree_bad_path &e) {
      /* This case is expected, the config does not have the requested key */
    } catch (KurentoException &e) {
      GST_WARNING ("Posible error deserializing %s from config", key.c_str() );
    } catch (std::exception &e) {
      GST_WARNING ("Unknown error getting%s from config", key.c_str() );
    }

    return defaultValue;
  }

  /*
   * This method is intented to perform initialization actions that require
   * a call to shared_from_this ()
   */
  virtual void postConstructor ();

  const boost::property_tree::ptree &config;

private:

  std::string initialId;
  std::string id;
  std::string name;
  std::recursive_mutex mutex;
  std::shared_ptr<MediaObject> parent;
  int64_t creationTime;

  std::string createId();

  std::map<std::string, std::string> tagsMap;
  bool sendTagsInEvents;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  friend Factory;
};

} /* kurento */

#endif /*  __MEDIA_OBJECT_IMPL_HPP__ */
