#ifndef __MEDIA_ELEMENT_IMPL_HPP__
#define __MEDIA_ELEMENT_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "MediaElement.hpp"
#include "MediaType.hpp"
#include <EventHandler.hpp>
#include <gst/gst.h>
#include <mutex>
#include <set>

namespace kurento
{

class MediaType;
class MediaElementImpl;
class AudioCodec;
class VideoCodec;

struct MediaTypeCmp {
  bool operator() (const std::shared_ptr<MediaType> &a,
                   const std::shared_ptr<MediaType> &b) const
  {
    return a->getValue () < b->getValue ();
  }
};

void Serialize (std::shared_ptr<MediaElementImpl> &object,
                JsonSerializer &serializer);

class ElementConnectionDataInternal;

class MediaElementImpl : public MediaObjectImpl, public virtual MediaElement
{

public:

  MediaElementImpl (const boost::property_tree::ptree &config,
                    std::shared_ptr<MediaObjectImpl> parent,
                    const std::string &factoryName);

  virtual ~MediaElementImpl ();

  GstElement *getGstreamerElement()
  {
    return element;
  };

  virtual std::vector<std::shared_ptr<ElementConnectionData>>
      getSourceConnections ();
  virtual std::vector<std::shared_ptr<ElementConnectionData>>
      getSourceConnections (
        std::shared_ptr<MediaType> mediaType);
  virtual std::vector<std::shared_ptr<ElementConnectionData>>
      getSourceConnections (
        std::shared_ptr<MediaType> mediaType, const std::string &description);
  virtual std::vector<std::shared_ptr<ElementConnectionData>>
      getSinkConnections ();
  virtual std::vector<std::shared_ptr<ElementConnectionData>> getSinkConnections (
        std::shared_ptr<MediaType> mediaType);
  virtual std::vector<std::shared_ptr<ElementConnectionData>> getSinkConnections (
        std::shared_ptr<MediaType> mediaType, const std::string &description);
  virtual void connect (std::shared_ptr<MediaElement> sink);
  virtual void connect (std::shared_ptr<MediaElement> sink,
                        std::shared_ptr<MediaType> mediaType);
  virtual void connect (std::shared_ptr<MediaElement> sink,
                        std::shared_ptr<MediaType> mediaType,
                        const std::string &sourceMediaDescription);
  virtual void connect (std::shared_ptr<MediaElement> sink,
                        std::shared_ptr<MediaType> mediaType,
                        const std::string &sourceMediaDescription,
                        const std::string &sinkMediaDescription);
  virtual void disconnect (std::shared_ptr<MediaElement> sink);
  virtual void disconnect (std::shared_ptr<MediaElement> sink,
                           std::shared_ptr<MediaType> mediaType);
  virtual void disconnect (std::shared_ptr<MediaElement> sink,
                           std::shared_ptr<MediaType> mediaType,
                           const std::string &sourceMediaDescription);
  virtual void disconnect (std::shared_ptr<MediaElement> sink,
                           std::shared_ptr<MediaType> mediaType,
                           const std::string &sourceMediaDescription,
                           const std::string &sinkMediaDescription);
  void setAudioFormat (std::shared_ptr<AudioCaps> caps);
  void setVideoFormat (std::shared_ptr<VideoCaps> caps);

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  GstElement *element;
  GstBus *bus;
  gulong handlerId;

private:
  std::recursive_mutex sourcesMutex;
  std::recursive_mutex sinksMutex;

  std::map<std::shared_ptr <MediaType>, std::map<std::string,
      std::shared_ptr<ElementConnectionDataInternal>>, MediaTypeCmp> sources;
  std::map<std::shared_ptr <MediaType>, std::map<std::string,
      std::set<std::shared_ptr<ElementConnectionDataInternal>>>, MediaTypeCmp>
      sinks;

  void disconnectAll();

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  friend void _media_element_impl_bus_message (GstBus *bus, GstMessage *message,
      gpointer data);
};

} /* kurento */

#endif /*  __MEDIA_ELEMENT_IMPL_HPP__ */
