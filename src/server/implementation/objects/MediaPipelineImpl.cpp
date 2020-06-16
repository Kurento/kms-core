/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <gst/gst.h>
#include <MediaPipelineImplFactory.hpp>
#include "MediaPipelineImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <DotGraph.hpp>
#include <GstreamerDotDetails.hpp>
#include <SignalHandler.hpp>
#include <memory>
#include "kmselement.h"

#define GST_CAT_DEFAULT kurento_media_pipeline_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoMediaPipelineImpl"

namespace kurento
{

void
MediaPipelineImpl::processBusMessage (GstMessage *msg)
{
  GstDebugLevel log_level = GST_LEVEL_NONE;
  GError *err = NULL;
  gchar *dbg_info = NULL;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      log_level = GST_LEVEL_ERROR;
      gst_message_parse_error (msg, &err, &dbg_info);
      break;
    case GST_MESSAGE_WARNING:
      log_level = GST_LEVEL_WARNING;
      gst_message_parse_warning (msg, &err, &dbg_info);
      break;
    default:
      return;
      break;
  }

  GstElement *parent = this->pipeline;
  gint err_code = 0;
  gchar *err_msg = NULL;

  if (err != NULL) {
    err_code = err->code;
    err_msg = err->message;
  }

  GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, log_level, NULL,
      "Error code %d: '%s', element: %s, parent: %s", err_code,
      GST_STR_NULL (err_msg), GST_MESSAGE_SRC_NAME (msg),
      GST_ELEMENT_NAME (parent));

  GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, log_level, NULL, "Debugging info: %s",
      GST_STR_NULL (dbg_info));

  std::string errorMessage (err_msg);
  if (dbg_info) {
    errorMessage += " (" + std::string (dbg_info) + ")";
  }

  gint code = err_code;
  try {
    Error event (shared_from_this (), errorMessage, code,
        "UNEXPECTED_PIPELINE_ERROR");
    sigcSignalEmit(signalError, event);
  } catch (const std::bad_weak_ptr &e) {
    // shared_from_this()
    GST_ERROR ("BUG creating %s: %s", Error::getName ().c_str (), e.what ());
  }

  gchar *dot_name = g_strdup_printf ("%s_bus_%d", GST_DEFAULT_NAME, err_code);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (parent), GST_DEBUG_GRAPH_SHOW_ALL,
      dot_name);
  g_free(dot_name);

  g_error_free (err);
  g_free (dbg_info);

  return;
}

void MediaPipelineImpl::postConstructor ()
{
  GstBus *bus;

  MediaObjectImpl::postConstructor ();

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline) );
  gst_bus_add_signal_watch (bus);
  busMessageHandler = register_signal_handler (G_OBJECT (bus), "message",
                      std::function <void (GstBus *, GstMessage *) > (std::bind (
                            &MediaPipelineImpl::processBusMessage, this,
                            std::placeholders::_2) ),
                      std::dynamic_pointer_cast<MediaPipelineImpl>
                      (shared_from_this() ) );
  g_object_unref (bus);
}

MediaPipelineImpl::MediaPipelineImpl (const boost::property_tree::ptree &config)
  : MediaObjectImpl (config)
{
  GstClock *clock;

  pipeline = gst_pipeline_new(nullptr);

  if (pipeline == nullptr) {
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

  gst_element_set_state (pipeline, GST_STATE_NULL);

  if (busMessageHandler > 0) {
    unregister_signal_handler (bus, busMessageHandler);
  }

  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
}

std::string MediaPipelineImpl::getGstreamerDot (
  std::shared_ptr<GstreamerDotDetails> details)
{
  return generateDotGraph (GST_BIN (pipeline), details);
}

std::string MediaPipelineImpl::getGstreamerDot()
{
  return generateDotGraph(
      GST_BIN(pipeline),
      std::make_shared<GstreamerDotDetails>(GstreamerDotDetails::SHOW_VERBOSE));
}

bool
MediaPipelineImpl::getLatencyStats ()
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);
  return latencyStats;
}

void
MediaPipelineImpl::setLatencyStats (bool latencyStats)
{
  GstIterator *it;
  gboolean done = FALSE;
  GValue item = G_VALUE_INIT;
  std::unique_lock <std::recursive_mutex> lock (recMutex);

  if (this->latencyStats == latencyStats) {
    return;
  }

  this->latencyStats = latencyStats;
  it = gst_bin_iterate_elements (GST_BIN (pipeline) );

  while (!done) {
    switch (gst_iterator_next (it, &item) ) {
    case GST_ITERATOR_OK: {
      GstElement *element = GST_ELEMENT (g_value_get_object (&item) );

      if (KMS_IS_ELEMENT (element) ) {
        g_object_set (element, "media-stats", latencyStats, NULL);
      }

      g_value_reset (&item);
      break;
    }

    case GST_ITERATOR_RESYNC:
      gst_iterator_resync (it);
      break;

    case GST_ITERATOR_ERROR:
    case GST_ITERATOR_DONE:
      done = TRUE;
      break;
    }
  }

  g_value_unset (&item);
  gst_iterator_free (it);
}

bool
MediaPipelineImpl::addElement (GstElement *element)
{
  std::unique_lock <std::recursive_mutex> lock (recMutex);
  bool ret;

  if (KMS_IS_ELEMENT (element) ) {
    g_object_set (element, "media-stats", latencyStats, NULL);
  }

  ret = gst_bin_add (GST_BIN (pipeline), element);

  if (ret) {
    gst_element_sync_state_with_parent (element);
  }

  return ret;
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
