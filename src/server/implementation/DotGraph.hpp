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

#ifndef __KMS_DOT_GRAPH_H__
#define __KMS_DOT_GRAPH_H__

#include <gst/gst.h>
#include <string>
#include <GstreamerDotDetails.hpp>

namespace kurento
{

std::string
generateDotGraph (GstBin *bin, std::shared_ptr<GstreamerDotDetails> details);

} /* kurento */

#endif /* __KMS_DOT_GRAPH_H__ */
