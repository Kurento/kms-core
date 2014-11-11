#ifndef __MEDIA_SINK_IMPL_HPP__
#define __MEDIA_SINK_IMPL_HPP__

#include "MediaPadImpl.hpp"
#include "MediaSink.hpp"
#include <EventHandler.hpp>
#include <mutex>

namespace kurento
{

class MediaSourceImpl;
class MediaSinkImpl;

void Serialize (std::shared_ptr<MediaSinkImpl> &object, JsonSerializer &serializer);

class MediaSinkImpl : public MediaPadImpl, public virtual MediaSink
{

public:

  MediaSinkImpl (const boost::property_tree::ptree &config,
                 std::shared_ptr<MediaType> mediaType,
                 const std::string &mediaDescription,
                 std::shared_ptr<MediaObjectImpl> parent);

  virtual ~MediaSinkImpl ();

  void disconnect (std::shared_ptr<MediaSource> src);
  std::shared_ptr<MediaSource> getConnectedSrc ();

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:
  std::string getPadName ();

  bool linkPad (std::shared_ptr<MediaSourceImpl> mediaSrc, GstPad *pad);
  void unlink (std::shared_ptr<MediaSourceImpl> mediaSrc, GstPad *sink);
  void unlinkUnchecked (GstPad *sink);

  std::weak_ptr <MediaSourceImpl> connectedSrc;

  std::recursive_mutex mutex;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  friend class MediaSourceImpl;
  friend gboolean link_media_pads (std::shared_ptr<MediaSourceImpl> src,
                                   std::shared_ptr<MediaSinkImpl> sink);
};

} /* kurento */

#endif /*  __MEDIA_SINK_IMPL_HPP__ */
