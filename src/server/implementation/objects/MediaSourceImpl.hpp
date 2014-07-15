#ifndef __MEDIA_SOURCE_IMPL_HPP__
#define __MEDIA_SOURCE_IMPL_HPP__

#include "MediaPadImpl.hpp"
#include "MediaSource.hpp"
#include <EventHandler.hpp>
#include <glibmm.h>

namespace kurento
{

class MediaSinkImpl;
class MediaSourceImpl;

void Serialize (std::shared_ptr<MediaSourceImpl> &object, JsonSerializer &serializer);

class MediaSourceImpl : public MediaPadImpl, public virtual MediaSource
{

public:

  MediaSourceImpl (std::shared_ptr<MediaType> mediaType,
                   const std::string &mediaDescription,
                   std::shared_ptr<MediaObjectImpl> parent);

  virtual ~MediaSourceImpl ();

  void connect (std::shared_ptr<MediaSink> sink);

  virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);

  class Factory : public virtual MediaPadImpl::Factory
  {
  public:
    Factory () {};

    virtual std::string getName () const {
      return "MediaSource";
    };

  };

  std::vector < std::shared_ptr<MediaSink> > getConnectedSinks ();

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:
  std::vector < std::weak_ptr<MediaSinkImpl> > connectedSinks;
  void removeSink (MediaSinkImpl *mediaSink);
  void disconnect (MediaSinkImpl *mediaSink);

  Glib::Threads::RecMutex mutex;

  const gchar *getPadName ();

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  friend class MediaSinkImpl;
  friend gboolean link_media_elements (std::shared_ptr<MediaSourceImpl> src,
                                       std::shared_ptr<MediaSinkImpl> sink);
};

} /* kurento */

#endif /*  __MEDIA_SOURCE_IMPL_HPP__ */
