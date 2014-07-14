#include <gst/gst.h>
#include "SdpEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_sdp_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoSdpEndpointImpl"

namespace kurento
{

SdpEndpointImpl::SdpEndpointImpl ()
{
  // FIXME: Implement this
}

std::string SdpEndpointImpl::generateOffer ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "SdpEndpointImpl::generateOffer: Not implemented");
}

std::string SdpEndpointImpl::processOffer (const std::string &offer)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "SdpEndpointImpl::processOffer: Not implemented");
}

std::string SdpEndpointImpl::processAnswer (const std::string &answer)
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "SdpEndpointImpl::processAnswer: Not implemented");
}

std::string SdpEndpointImpl::getLocalSessionDescriptor ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "SdpEndpointImpl::getLocalSessionDescriptor: Not implemented");
}

std::string SdpEndpointImpl::getRemoteSessionDescriptor ()
{
  // FIXME: Implement this
  throw KurentoException (NOT_IMPLEMENTED, "SdpEndpointImpl::getRemoteSessionDescriptor: Not implemented");
}

SdpEndpointImpl::StaticConstructor SdpEndpointImpl::staticConstructor;

SdpEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
