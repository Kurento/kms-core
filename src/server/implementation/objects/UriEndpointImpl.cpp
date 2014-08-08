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

typedef enum {
  KMS_URI_END_POINT_STATE_STOP,
  KMS_URI_END_POINT_STATE_START,
  KMS_URI_END_POINT_STATE_PAUSE
} KmsUriEndPointState;

UriEndpointImpl::UriEndpointImpl (const boost::property_tree::ptree &config,
                                  std::shared_ptr< MediaObjectImpl > parent,
                                  const std::string &factoryName, const std::string &uri) :
  EndpointImpl (config, parent, factoryName)
{
  this->uri = uri;
  g_object_set (G_OBJECT (getGstreamerElement() ), "uri", uri.c_str(), NULL);
}

void UriEndpointImpl::pause ()
{
  g_object_set (G_OBJECT (getGstreamerElement() ), "state",
                KMS_URI_END_POINT_STATE_PAUSE, NULL);
}

void UriEndpointImpl::stop ()
{
  g_object_set (G_OBJECT (getGstreamerElement() ), "state",
                KMS_URI_END_POINT_STATE_STOP, NULL);
}

void
UriEndpointImpl::start ()
{
  g_object_set (G_OBJECT (getGstreamerElement() ), "state",
                KMS_URI_END_POINT_STATE_START, NULL);
}

std::string
UriEndpointImpl::getUri ()
{
  return uri;
}

UriEndpointImpl::StaticConstructor UriEndpointImpl::staticConstructor;

UriEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
