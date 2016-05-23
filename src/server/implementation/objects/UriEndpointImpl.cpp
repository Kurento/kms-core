#include <gst/gst.h>
#include "UriEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <boost/regex.hpp>

#define GST_CAT_DEFAULT kurento_uri_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoUriEndpointImpl"

#define DEFAULT_PATH "defaultPath"
#define DEFAULT_PATH_VALUE "file:///var/kurento"

namespace kurento
{

typedef enum {
  KMS_URI_END_POINT_STATE_STOP,
  KMS_URI_END_POINT_STATE_START,
  KMS_URI_END_POINT_STATE_PAUSE
} KmsUriEndPointState;

void UriEndpointImpl::removeDuplicateSlashes (std::string &uri)
{
  boost::regex re ("//*");
  gchar *location, *protocol;

  location = gst_uri_get_location (uri.c_str () );
  protocol = gst_uri_get_protocol (uri.c_str () );

  std::string uriWithoutProtocol (location );
  std::string uriProtocol (protocol );

  g_free (location);
  g_free (protocol);

  // if protocol is file:/// glib only remove the two first slashes and we need
  // to remove the last one.
  if (uriWithoutProtocol[0] == '/') {
    uriWithoutProtocol.erase (0, 1);
  }

  std::string uriWithoutSlash (boost::regex_replace (uriWithoutProtocol, re,
                               "/") );

  if (uriProtocol == "file") {
    uri = uriProtocol + ":///" + uriWithoutSlash;
  } else {
    uri = uriProtocol + "://" + uriWithoutSlash;
  }
}

void UriEndpointImpl::checkUri ()
{
  this->absolute_uri = this->uri;

  //Check if uri is an absolute or relative path.
  if (! (gst_uri_is_valid (this->absolute_uri.c_str () ) ) ) {
    std::string path;

    path = getConfigValue <std::string, UriEndpoint> (DEFAULT_PATH,
           DEFAULT_PATH_VALUE);

    this->absolute_uri = path + "/" + this->uri;
  }

  removeDuplicateSlashes (this->absolute_uri);
}

UriEndpointImpl::UriEndpointImpl (const boost::property_tree::ptree &config,
                                  std::shared_ptr< MediaObjectImpl > parent,
                                  const std::string &factoryName, const std::string &uri) :
  EndpointImpl (config, parent, factoryName)
{
  this->uri = uri;
  checkUri();
  g_object_set (G_OBJECT (getGstreamerElement() ), "uri",
                this->absolute_uri.c_str(),
                NULL);
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
