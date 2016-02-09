#ifndef __MEDIA_ELEMENT_IMPL_HPP__
#define __MEDIA_ELEMENT_IMPL_HPP__

#include "MediaObjectImpl.hpp"
#include "MediaElement.hpp"
#include "MediaType.hpp"
#include "MediaLatencyStat.hpp"
#include <EventHandler.hpp>
#include <gst/gst.h>
#include <mutex>
#include <set>
#include <random>
#include "MediaFlowOutStateChange.hpp"
#include "MediaFlowInStateChange.hpp"
#include "MediaFlowState.hpp"
#include "commons/kmselement.h"

namespace kurento
{

class MediaType;
class MediaElementImpl;
class AudioCodec;
class VideoCodec;
class MediaFlowData;

struct MediaTypeCmp {
  bool operator() (const std::shared_ptr<MediaType> &a,
                   const std::shared_ptr<MediaType> &b) const {
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

  GstElement *getGstreamerElement() {
    return element;
  };

  virtual std::map <std::string, std::shared_ptr<Stats>> getStats ();
  virtual std::map <std::string, std::shared_ptr<Stats>> getStats (
        std::shared_ptr<MediaType> mediaType);

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

  virtual void release ();

  virtual std::string getGstreamerDot ();
  virtual std::string getGstreamerDot (std::shared_ptr<GstreamerDotDetails>
                                       details);

  virtual void setOutputBitrate (int bitrate);

  bool isMediaFlowingIn (std::shared_ptr<MediaType> mediaType);
  bool isMediaFlowingIn (std::shared_ptr<MediaType> mediaType,
                         const std::string &sinkMediaDescription);
  bool isMediaFlowingOut (std::shared_ptr<MediaType> mediaType);
  bool isMediaFlowingOut (std::shared_ptr<MediaType> mediaType,
                          const std::string &sourceMediaDescription);

  virtual int getMinOuputBitrate ();
  virtual void setMinOuputBitrate (int minOuputBitrate);

  virtual int getMaxOuputBitrate ();
  virtual void setMaxOuputBitrate (int maxOuputBitrate);

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  sigc::signal<void, ElementConnected> signalElementConnected;
  sigc::signal<void, ElementDisconnected> signalElementDisconnected;
  sigc::signal<void, MediaFlowOutStateChange> signalMediaFlowOutStateChange;
  sigc::signal<void, MediaFlowInStateChange> signalMediaFlowInStateChange;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  GstElement *element;
  GstBus *bus;
  gulong handlerId;

  virtual void postConstructor ();
  void collectLatencyStats (std::vector<std::shared_ptr<MediaLatencyStat>>
                            &latencyStats, const GstStructure *stats);
  virtual void fillStatsReport (std::map <std::string, std::shared_ptr<Stats>>
                                &report, const GstStructure *stats, double timestamp);

private:
  std::recursive_timed_mutex sourcesMutex;
  std::recursive_timed_mutex sinksMutex;

  std::map < std::shared_ptr <MediaType>, std::map < std::string,
      std::shared_ptr<ElementConnectionDataInternal >> , MediaTypeCmp > sources;
  std::map < std::shared_ptr <MediaType>, std::map < std::string,
      std::set<std::shared_ptr<ElementConnectionDataInternal> >> , MediaTypeCmp >
      sinks;

  std::mt19937_64 rnd {std::random_device{}() };
  std::uniform_int_distribution<> dist {1, 100};

  gulong padAddedHandlerId;
  gulong mediaFlowOutHandler;
  gulong mediaFlowInHandler;
  std::map <std::string, std::shared_ptr <MediaFlowData>> mediaFlowDataIn;
  std::map <std::string, std::shared_ptr <MediaFlowData>> mediaFlowDataOut;

  void disconnectAll();
  void performConnection (std::shared_ptr <ElementConnectionDataInternal> data);
  std::map <std::string, std::shared_ptr<Stats>> generateStats (
        const gchar *selector);
  void mediaFlowOutStateChange (gboolean isFlowing, gchar *padName,
                                KmsElementPadType type);
  void mediaFlowInStateChange (gboolean isFlowing, gchar *padName,
                               KmsElementPadType type);

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  friend void _media_element_impl_bus_message (GstBus *bus, GstMessage *message,
      gpointer data);
  friend void _media_element_pad_added (GstElement *elem, GstPad *pad,
                                        gpointer data);
};

} /* kurento */

#endif /*  __MEDIA_ELEMENT_IMPL_HPP__ */
