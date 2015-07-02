#include <gst/gst.h>
#include <MediaPipelineImplFactory.hpp>
#include "MediaPipelineImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <DotGraph.hpp>
#include <GstreamerDotDetails.hpp>
#include <SignalHandler.hpp>

#define GST_CAT_DEFAULT kurento_media_pipeline_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaPipelineImpl"

namespace kurento
{
void
MediaPipelineImpl::busMessage (GstMessage *message)
{
  switch (message->type) {
  case GST_MESSAGE_ERROR: {
    GError *err = NULL;
    gchar *debug = NULL;

    GST_ERROR ("Error on bus: %" GST_PTR_FORMAT, message);
    gst_debug_bin_to_dot_file_with_ts (GST_BIN (pipeline),
                                       GST_DEBUG_GRAPH_SHOW_ALL, "error");
    gst_message_parse_error (message, &err, &debug);
    std::string errorMessage (err->message);

    if (debug != NULL) {
      errorMessage += " -> " + std::string (debug);
    }

    try {
      Error error (shared_from_this(), errorMessage , 0,
                   "UNEXPECTED_PIPELINE_ERROR");

      signalError (error);
    } catch (std::bad_weak_ptr &e) {
    }

    g_error_free (err);
    g_free (debug);
    break;
  }

  default:
    break;
  }
}

void MediaPipelineImpl::postConstructor ()
{
  GstBus *bus;

  MediaObjectImpl::postConstructor ();

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline) );
  gst_bus_add_signal_watch (bus);
  busMessageHandler = register_signal_handler (G_OBJECT (bus), "message",
                      std::function <void (GstBus *, GstMessage *) > (std::bind (
                            &MediaPipelineImpl::busMessage, this,
                            std::placeholders::_2) ),
                      std::dynamic_pointer_cast<MediaPipelineImpl>
                      (shared_from_this() ) );
  g_object_unref (bus);
}

MediaPipelineImpl::MediaPipelineImpl (const boost::property_tree::ptree &config)
  : MediaObjectImpl (config)
{
  GstClock *clock;

  pipeline = gst_pipeline_new (NULL);

  if (pipeline == NULL) {
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Cannot create gstreamer pipeline");
  }

  clock = gst_system_clock_obtain ();
  gst_pipeline_use_clock (GST_PIPELINE (pipeline), clock);
  g_object_unref (clock);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  busMessageHandler = 0;
}

MediaPipelineImpl::~MediaPipelineImpl ()
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline) );

  if (busMessageHandler > 0) {
    unregister_signal_handler (bus, busMessageHandler);
  }

  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);
}

std::string MediaPipelineImpl::getGstreamerDot (
  std::shared_ptr<GstreamerDotDetails> details)
{
  switch (details->getValue() ) {
  case GstreamerDotDetails::SHOW_MEDIA_TYPE:
    return generateDotGraph (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE);

  case GstreamerDotDetails::SHOW_CAPS_DETAILS:
    return generateDotGraph (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS);

  case GstreamerDotDetails::SHOW_NON_DEFAULT_PARAMS:
    return generateDotGraph (GST_BIN (pipeline),
                             GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);

  case GstreamerDotDetails::SHOW_STATES:
    return generateDotGraph (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_STATES);

  case GstreamerDotDetails::SHOW_ALL:
  default:
    return generateDotGraph (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_ALL);
  }
}

std::string MediaPipelineImpl::getGstreamerDot()
{
  return generateDotGraph (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_ALL);
}

MediaObjectImpl *
MediaPipelineImplFactory::createObject (const boost::property_tree::ptree &pt)
const
{
  return new MediaPipelineImpl (pt);
}

MediaPipelineImpl::StaticConstructor MediaPipelineImpl::staticConstructor;

MediaPipelineImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
