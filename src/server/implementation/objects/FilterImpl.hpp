#ifndef __FILTER_IMPL_HPP__
#define __FILTER_IMPL_HPP__

#include "MediaElementImpl.hpp"
#include "Filter.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class FilterImpl;

void Serialize (std::shared_ptr<FilterImpl> &object,
                JsonSerializer &serializer);

class FilterImpl : public MediaElementImpl, public virtual Filter
{

public:

  FilterImpl (const boost::property_tree::ptree &config,
              std::shared_ptr<MediaObjectImpl> parent);

  virtual ~FilterImpl () {};

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __FILTER_IMPL_HPP__ */
