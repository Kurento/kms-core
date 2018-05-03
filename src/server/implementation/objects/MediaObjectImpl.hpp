/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __MEDIA_OBJECT_IMPL_HPP__
#define __MEDIA_OBJECT_IMPL_HPP__

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
class MediaSet;

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
  virtual std::vector<std::shared_ptr<MediaObject>> getChildren ();

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

  template <class T>
  static T getConfigValue (const boost::property_tree::ptree &config,
                           const std::string &key)
  {
    auto child = config.get_child (key);
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

  template <class T>
  static T getConfigValue (const boost::property_tree::ptree &config,
                           const std::string &key, T defaultValue)
  {
    try {
      return getConfigValue<T> (config, key);
    } catch (boost::property_tree::ptree_bad_path &e) {
      /* This case is expected, the config does not have the requested key */
    } catch (KurentoException &e) {
      GST_WARNING ("Posible error deserializing %s from config", key.c_str() );
    } catch (std::exception &e) {
      GST_WARNING ("Unknown error getting '%s' from config", key.c_str() );
    }

    return defaultValue;
  }

protected:

  template <class T>
  T getConfigValue (const std::string &key)
  {
    return getConfigValue <T> (config, key);
  }

  template <class T>
  T getConfigValue (const std::string &key, T defaultValue)
  {
    return getConfigValue <T> (config, key, defaultValue);
  }

  template <class T, class C>
  T getConfigValue (const std::string &key)
  {
    return getConfigValue <T> ("modules." + dynamic_cast <C *>
                               (this)->getModule() + "."
                               + dynamic_cast <C *> (this)->getType() + "." + key);
  }

  template <class T, class C>
  T getConfigValue (const std::string &key, T defaultValue)
  {
    return getConfigValue <T> ("modules." + dynamic_cast <C *>
                               (this)->getModule() + "."
                               + dynamic_cast <C *> (this)->getType() + "." + key, defaultValue);
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

  friend MediaSet;
};

} /* kurento */

#endif /*  __MEDIA_OBJECT_IMPL_HPP__ */
