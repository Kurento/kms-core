#include <gst/gst.h>
#include "HubImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <MediaPipelineImpl.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_hub_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoHubImpl"

namespace kurento
{

HubImpl::HubImpl (const boost::property_tree::ptree &config,
                  std::shared_ptr<MediaObjectImpl> parent,
                  const std::string &factoryName) : MediaObjectImpl (config, parent)
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  element = gst_element_factory_make (factoryName.c_str(), NULL);

  g_object_ref (element);
  gst_bin_add (GST_BIN ( pipe->getPipeline() ), element);
  gst_element_sync_state_with_parent (element);
}

HubImpl::~HubImpl()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  gst_bin_remove (GST_BIN ( pipe->getPipeline() ), element);
  gst_element_set_state (element, GST_STATE_NULL);
  g_object_unref (element);
}

HubImpl::StaticConstructor HubImpl::staticConstructor;

HubImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
