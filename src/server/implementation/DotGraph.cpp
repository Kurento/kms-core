/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include "DotGraph.hpp"
#include <string.h>

namespace kurento
{

static GstDebugGraphDetails
convert_details (std::shared_ptr<GstreamerDotDetails> details)
{
  switch (details->getValue() ) {
  case GstreamerDotDetails::SHOW_MEDIA_TYPE:
    return GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE;

  case GstreamerDotDetails::SHOW_CAPS_DETAILS:
    return GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS;

  case GstreamerDotDetails::SHOW_NON_DEFAULT_PARAMS:
    return GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS;

  case GstreamerDotDetails::SHOW_STATES:
    return GST_DEBUG_GRAPH_SHOW_STATES;

  case GstreamerDotDetails::SHOW_FULL_PARAMS:
    return GST_DEBUG_GRAPH_SHOW_FULL_PARAMS;

  case GstreamerDotDetails::SHOW_ALL:
    return GST_DEBUG_GRAPH_SHOW_ALL;

  case GstreamerDotDetails::SHOW_VERBOSE:
  default:
    return GST_DEBUG_GRAPH_SHOW_VERBOSE;
  }
}

std::string
generateDotGraph (GstBin *bin, std::shared_ptr<GstreamerDotDetails> details)
{
  std::string retString;
  gchar *data = gst_debug_bin_to_dot_data (bin, convert_details (details) );

  retString = std::string (data);
  g_free (data);

  return retString;
}

} /* kurento */
