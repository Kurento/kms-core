#include <gst/gst.h>
#include "FilterImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>

#define GST_CAT_DEFAULT kurento_filter_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoFilterImpl"

#define FACTORY_NAME "filterelement"

namespace kurento
{

FilterImpl::FilterImpl (std::shared_ptr<MediaObjectImpl> parent) :
  MediaElementImpl (parent, FACTORY_NAME)
{
}

FilterImpl::StaticConstructor FilterImpl::staticConstructor;

FilterImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
