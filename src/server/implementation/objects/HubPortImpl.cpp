#include <gst/gst.h>
#include "HubImpl.hpp"
#include <HubPortImplFactory.hpp>
#include "HubPortImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_hub_port_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoHubPortImpl"

#define FACTORY_NAME "hubport"

namespace kurento
{

HubPortImpl::HubPortImpl (const boost::property_tree::ptree &config, std::shared_ptr<HubImpl> hub) : MediaElementImpl (config, hub, FACTORY_NAME)
{
  g_signal_emit_by_name (hub->getGstreamerElement(), "handle-port",
                         element, &handlerId);
}

HubPortImpl::~HubPortImpl()
{
  g_signal_emit_by_name (std::dynamic_pointer_cast<HubImpl> (getParent() )->getGstreamerElement(),
                         "unhandle-port", handlerId);
}

MediaObjectImpl *
HubPortImplFactory::createObject (const boost::property_tree::ptree &conf, std::shared_ptr<Hub> hub) const
{
  return new HubPortImpl (conf, std::dynamic_pointer_cast<HubImpl> (hub) );
}

HubPortImpl::StaticConstructor HubPortImpl::staticConstructor;

HubPortImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
