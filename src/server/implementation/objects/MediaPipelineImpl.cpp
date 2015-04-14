#include <gst/gst.h>
#include <MediaPipelineImplFactory.hpp>
#include "MediaPipelineImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <DotGraph.hpp>
#include <GstreamerDotDetails.hpp>

#define GST_CAT_DEFAULT kurento_media_pipeline_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaPipelineImpl"

namespace kurento
{

class MessageAdaptorAux;

class MessageAdaptorAux
{
public:
  MessageAdaptorAux (std::function <void (GstMessage *) > func) : func (func)
  {

  }

  std::function <void (GstMessage *) > func;
};

static void
bus_message_adaptor (GstBus *bus, GstMessage *message, gpointer data)
{
  MessageAdaptorAux *adaptor = (MessageAdaptorAux *) data;

  adaptor->func (message);
}

static void
message_adaptor_destroy (gpointer d, GClosure *closure)
{
  MessageAdaptorAux *data = (MessageAdaptorAux *) d;

  delete data;
}

static gulong
register_bus_messages (GstBus *bus,
                       std::function <void (GstMessage *message) > func)
{
  gulong id;

  MessageAdaptorAux *data = new MessageAdaptorAux (func);

  id = g_signal_connect_data (bus, "message", G_CALLBACK (bus_message_adaptor),
                              (gpointer) data, message_adaptor_destroy, (GConnectFlags) 0);
  return id;
}

static void
unregister_bus_messages (GstBus *bus, gulong id)
{
  g_signal_handler_disconnect (bus, id);
}

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

MediaPipelineImpl::MediaPipelineImpl (const boost::property_tree::ptree &config)
  : MediaObjectImpl (config)
{
  GstBus *bus;
  GstClock *clock;

  pipeline = gst_pipeline_new (NULL);

  if (pipeline == NULL) {
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Cannot create gstreamer pipeline");
  }

  g_object_set (G_OBJECT (pipeline), "async-handling", TRUE, NULL);

  clock = gst_system_clock_obtain ();
  gst_pipeline_use_clock (GST_PIPELINE (pipeline), clock);
  g_object_unref (clock);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline) );
  gst_bus_add_signal_watch (bus);
  busMessageHandler = register_bus_messages (bus,
                      std::bind (&MediaPipelineImpl::busMessage, this,
                                 std::placeholders::_1) );
  g_object_unref (bus);
}

MediaPipelineImpl::~MediaPipelineImpl ()
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline) );

  unregister_bus_messages (bus, busMessageHandler);
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
