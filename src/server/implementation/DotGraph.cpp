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

std::string
generateDotGraph (GstBin *bin, GstDebugGraphDetails details)
{
  std::string retString;
  gchar *data = gst_debug_bin_to_dot_data (bin, details);

  retString = std::string (data);
  g_free (data);

  return retString;
}

} /* kurento */
