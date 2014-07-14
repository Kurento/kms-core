#include <gst/gst.h>
#include "SessionEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_session_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoSessionEndpointImpl"

namespace kurento
{

SessionEndpointImpl::SessionEndpointImpl ()
{
  // FIXME: Implement this
}

SessionEndpointImpl::StaticConstructor SessionEndpointImpl::staticConstructor;

SessionEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
