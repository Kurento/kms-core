#include <gst/gst.h>
#include "HubImpl.hpp"
#include "HubPortImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_hub_port_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoHubPortImpl"

namespace kurento
{

HubPortImpl::HubPortImpl (std::shared_ptr<Hub> hub)
{
  // FIXME: Implement this
}

MediaObjectImpl *
HubPortImpl::Factory::createObject (std::shared_ptr<Hub> hub) const
{
  return new HubPortImpl (hub);
}

HubPortImpl::StaticConstructor HubPortImpl::staticConstructor;

HubPortImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
