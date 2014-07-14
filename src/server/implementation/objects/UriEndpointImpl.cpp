#include <gst/gst.h>
#include "UriEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_uri_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoUriEndpointImpl"

namespace kurento
{

UriEndpointImpl::UriEndpointImpl ()
{
  // FIXME: Implement this
}

std::string UriEndpointImpl::getUri ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "UriEndpointImpl::getUri: Not implemented");
}

void UriEndpointImpl::pause ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "UriEndpointImpl::pause: Not implemented");
}

void UriEndpointImpl::stop ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "UriEndpointImpl::stop: Not implemented");
}

UriEndpointImpl::StaticConstructor UriEndpointImpl::staticConstructor;

UriEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
