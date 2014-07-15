#include <gst/gst.h>
#include "EndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoEndpointImpl"

namespace kurento
{

EndpointImpl::EndpointImpl ()
{
  throw KurentoException (MEDIA_OBJECT_CONSTRUCTOR_NOT_FOUND, "Default constructor of Endpoint should not be used");
}

EndpointImpl::EndpointImpl (std::shared_ptr< MediaObjectImpl > parent,
                            const std::string &factoryName) :
  MediaElementImpl (parent, factoryName)
{
}

EndpointImpl::StaticConstructor EndpointImpl::staticConstructor;

EndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
