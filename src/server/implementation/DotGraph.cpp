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

static std::string
debug_dump_get_element_state (GstElement *element)
{
  static std::string state_icons[] = {"[~]", "[0]", "[-]", "[=]", "[>]"};

  GstState state = GST_STATE_VOID_PENDING;
  GstState pending = GST_STATE_VOID_PENDING;

  gst_element_get_state (element, &state, &pending, 0);

  if (pending == GST_STATE_VOID_PENDING) {
    gboolean is_locked = gst_element_is_locked_state (element);

    return "\\n" + state_icons[state] + (is_locked ? "(locked)" : "");
  } else {
    return "\\n" + state_icons[state] + " -> " + state_icons[pending];
  }
}

static std::string
debug_dump_get_element_params (GstElement *element)
{
  std::string params;
  GParamSpec **properties;
  GValue value = { 0, };
  guint number_of_properties;

  /* get paramspecs and show non-default properties */
  properties =
    g_object_class_list_properties (G_OBJECT_CLASS (GST_ELEMENT_GET_CLASS
                                    (element) ), &number_of_properties);

  if (properties) {
    guint i;

    for (i = 0; i < number_of_properties; i++) {
      GParamSpec *property;

      property = properties[i];

      if (! (property->flags & G_PARAM_READABLE) ) {
        continue;
      }

      if (!strcmp (property->name, "name") ) {
        continue;
      }

      g_value_init (&value, property->value_type);
      g_object_get_property (G_OBJECT (element), property->name, &value);

      if (! (g_param_value_defaults (property, &value) ) ) {
        gchar *tmp, *value_str;

        tmp = g_strdup_value_contents (&value);
        value_str = g_strescape (tmp, NULL);
        g_free (tmp);

        params += "\\n" + std::string (property->name) + "=" + std::string (value_str);
        g_free (value_str);
      }

      g_value_unset (&value);
    }

    g_free (properties);
  }

  return params;
}

static std::string
debug_dump_make_object_name (GstObject *obj)
{
  gchar *c_str = g_strcanon (g_strdup_printf ("%s_%p", GST_OBJECT_NAME (obj),
                             obj),
                             G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "_", '_');
  std::string ret (c_str);

  g_free (c_str);

  return ret;
}

static std::string
debug_dump_pad (GstPad *pad, const std::string &colorName,
                const std::string &elementName, GstDebugGraphDetails details,
                const std::string &indentStr)
{
  std::string ret;
  GstPadTemplate *pad_templ;
  GstPadPresence presence;
  std::string padName;
  std::string styleName;

  padName = debug_dump_make_object_name (GST_OBJECT (pad) );

  /* pad availability */
  styleName = "filled,solid";

  if ( (pad_templ = gst_pad_get_pad_template (pad) ) ) {
    presence = GST_PAD_TEMPLATE_PRESENCE (pad_templ);
    gst_object_unref (pad_templ);

    if (presence == GST_PAD_SOMETIMES) {
      styleName = "filled,dotted";
    } else if (presence == GST_PAD_REQUEST) {
      styleName = "filled,dashed";
    }
  }

  if (details & GST_DEBUG_GRAPH_SHOW_STATES) {
    static std::string activationMode[] = {"-", ">", "<" };
    std::string padFlags;
    std::string taskMode;
    GstTask *task;

    GST_OBJECT_LOCK (pad);
    task = GST_PAD_TASK (pad);

    if (task) {
      switch (gst_task_get_state (task) ) {
      case GST_TASK_STARTED:
        taskMode = "[T]";
        break;

      case GST_TASK_PAUSED:
        taskMode = "[t]";
        break;

      default:
        /* Invalid task state, ignoring */
        break;
      }
    }

    GST_OBJECT_UNLOCK (pad);

    /* check if pad flags */
    padFlags.append (GST_OBJECT_FLAG_IS_SET (pad,
                     GST_PAD_FLAG_BLOCKED) ? "B" : "b");
    padFlags.append (GST_OBJECT_FLAG_IS_SET (pad,
                     GST_PAD_FLAG_FLUSHING) ? "F" : "f");
    padFlags.append (GST_OBJECT_FLAG_IS_SET (pad,
                     GST_PAD_FLAG_BLOCKING) ? "B" : "b");

    ret += indentStr + "  " + elementName + "_" + padName +
           " [color=black, fillcolor=\"" + colorName + "\", label=\"" + std::string (
             GST_OBJECT_NAME (pad) ) + "\\n[" + activationMode[pad->mode] + "][" + padFlags +
           "]" + taskMode + "\", height=\"0.2\", style=\"" + styleName + "\"];\n";
  } else {
    ret += indentStr + "  " + elementName + "_" + padName +
           " [color=black, fillcolor=\"" + colorName + "\", label=\"" + std::string (
             GST_OBJECT_NAME (pad) ) + "\", height=\"0.2\", style=\"" + styleName + "\"];\n";
  }

  return ret;
}

static std::string
debug_dump_element_pad (GstPad *pad, GstElement *element,
                        GstDebugGraphDetails details, const gint indent)
{
  std::string ret;

  GstElement *target_element;
  GstPad *target_pad, *tmp_pad;
  GstPadDirection dir;
  std::string elementName;
  std::string targetElementName;
  std::string colorName;
  std::string indentStr (indent * 2, ' ');

  dir = gst_pad_get_direction (pad);
  elementName = debug_dump_make_object_name (GST_OBJECT (element) );

  if (GST_IS_GHOST_PAD (pad) ) {
    colorName =
      (dir == GST_PAD_SRC) ? "#ffdddd" : ( (dir ==
                                            GST_PAD_SINK) ? "#ddddff" : "#ffffff");

    /* output target-pad so that it belongs to this element */
    if ( (tmp_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad) ) ) ) {
      if ( (target_pad = gst_pad_get_peer (tmp_pad) ) ) {
        std::string padName;
        std::string targetPadName;

        if ( (target_element = gst_pad_get_parent_element (target_pad) ) ) {
          targetElementName =
            debug_dump_make_object_name (GST_OBJECT (target_element) );
        }

        ret += debug_dump_pad (target_pad, colorName, targetElementName, details,
                               indentStr);
        /* src ghostpad relationship */
        padName = debug_dump_make_object_name (GST_OBJECT (pad) );
        targetPadName = debug_dump_make_object_name (GST_OBJECT (target_pad) );

        if (dir == GST_PAD_SRC) {
          ret += indentStr + targetElementName + "_" + targetPadName + " -> " +
                 elementName + "_" + padName + " [style=dashed, minlen=0]\n";
        } else {
          ret += indentStr + elementName + "_" + padName + " -> " + targetElementName +
                 "_" + targetPadName + " [style=dashed, minlen=0]\n";
        }

        if (target_element) {
          gst_object_unref (target_element);
        }

        gst_object_unref (target_pad);
      }

      gst_object_unref (tmp_pad);
    }
  } else {
    colorName =
      (dir == GST_PAD_SRC) ? "#ffaaaa" : ( (dir ==
                                            GST_PAD_SINK) ? "#aaaaff" : "#cccccc");
  }

  /* pads */
  ret += debug_dump_pad (pad, colorName, elementName, details, indentStr);

  return ret;
}

static std::string
debug_dump_element_pads (GstIterator *pad_iter, GstPad *pad,
                         GstElement *element, GstDebugGraphDetails details, const gint indent,
                         guint *src_pads, guint *sink_pads)
{
  GValue item = { 0, };
  gboolean pads_done;
  GstPadDirection dir;
  std::string ret;

  pads_done = FALSE;

  while (!pads_done) {
    switch (gst_iterator_next (pad_iter, &item) ) {
    case GST_ITERATOR_OK:
      pad = GST_PAD (g_value_get_object (&item) );
      ret += debug_dump_element_pad (pad, element, details, indent);
      dir = gst_pad_get_direction (pad);

      if (dir == GST_PAD_SRC) {
        (*src_pads)++;
      } else if (dir == GST_PAD_SINK) {
        (*sink_pads)++;
      }

      g_value_reset (&item);
      break;

    case GST_ITERATOR_RESYNC:
      gst_iterator_resync (pad_iter);
      break;

    case GST_ITERATOR_ERROR:
    case GST_ITERATOR_DONE:
      pads_done = TRUE;
      break;
    }
  }

  return ret;
}

static gboolean
string_append_field (GQuark field, const GValue *value, gpointer ptr)
{
  std::string *str = (std::string *) ptr;
  gchar *value_str = gst_value_serialize (value);
  gchar *esc_value_str;

  if (value_str == NULL) {
    gchar *c_tmp = g_strdup_printf ("  %18s: NULL\\l", g_quark_to_string (field) );
    std::string tmp (c_tmp);
    g_free (c_tmp);
    str->append (tmp);
    return TRUE;
  }

  /* some enums can become really long */
  if (strlen (value_str) > 25) {
    gint pos = 24;

    /* truncate */
    value_str[25] = '\0';

    /* mirror any brackets and quotes */
    if (value_str[0] == '<') {
      value_str[pos--] = '>';
    }

    if (value_str[0] == '[') {
      value_str[pos--] = ']';
    }

    if (value_str[0] == '(') {
      value_str[pos--] = ')';
    }

    if (value_str[0] == '{') {
      value_str[pos--] = '}';
    }

    if (value_str[0] == '"') {
      value_str[pos--] = '"';
    }

    if (pos != 24) {
      value_str[pos--] = ' ';
    }

    /* elippsize */
    value_str[pos--] = '.';
    value_str[pos--] = '.';
    value_str[pos--] = '.';
  }

  esc_value_str = g_strescape (value_str, NULL);

  gchar *c_tmp = g_strdup_printf ("  %18s: %s\\l", g_quark_to_string (field),
                                  esc_value_str);

  std::string tmp (c_tmp);
  g_free (c_tmp);

  str->append (tmp);

  g_free (value_str);
  g_free (esc_value_str);
  return TRUE;
}

static std::string
debug_dump_describe_caps (GstCaps *caps, GstDebugGraphDetails details)
{
  std::string media;

  if (details & GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS) {

    if (gst_caps_is_any (caps) || gst_caps_is_empty (caps) ) {
      gchar *tmp = gst_caps_to_string (caps);

      media = tmp;
      g_free (tmp);

    } else {
      std::string str;

      for (guint i = 0; i < gst_caps_get_size (caps); i++) {
        GstStructure *structure = gst_caps_get_structure (caps, i);

        str.append (gst_structure_get_name (structure) );
        str.append ("\\l");

        gst_structure_foreach (structure, string_append_field, (gpointer) &str);
      }

      media = str;
    }

  } else {
    if (GST_CAPS_IS_SIMPLE (caps) ) {
      media = gst_structure_get_name (gst_caps_get_structure (caps, 0) );
    } else {
      media = "*";
    }
  }

  return media;
}

static std::string
debug_dump_element_pad_link (GstPad *pad, GstElement *element,
                             GstDebugGraphDetails details, const std::string &indentStr)
{
  std::string ret;
  GstElement *peer_element;
  GstPad *peer_pad;
  GstCaps *caps, *peer_caps;
  std::string media;
  std::string mediaSrc;
  std::string mediaSink;
  std::string padName;
  std::string elementName;
  std::string peerPadName;
  std::string peerElementName;

  if ( (peer_pad = gst_pad_get_peer (pad) ) ) {
    if ( (details & GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE) ||
         (details & GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS)
       ) {
      caps = gst_pad_get_current_caps (pad);

      if (!caps) {
        caps = gst_pad_get_pad_template_caps (pad);
      }

      peer_caps = gst_pad_get_current_caps (peer_pad);

      if (!peer_caps) {
        peer_caps = gst_pad_get_pad_template_caps (peer_pad);
      }

      media = debug_dump_describe_caps (caps, details);

      /* check if peer caps are different */
      if (peer_caps && !gst_caps_is_equal (caps, peer_caps) ) {

        if (gst_pad_get_direction (pad) == GST_PAD_SRC) {
          mediaSrc = media;
          mediaSink = debug_dump_describe_caps (peer_caps, details);;
        } else {
          mediaSrc = debug_dump_describe_caps (peer_caps, details);;
          mediaSink = media;
        }

        media = "";
      }

      gst_caps_unref (peer_caps);
      gst_caps_unref (caps);
    }

    padName = debug_dump_make_object_name (GST_OBJECT (pad) );

    if (element) {
      elementName = debug_dump_make_object_name (GST_OBJECT (element) );
    }

    peerPadName = debug_dump_make_object_name (GST_OBJECT (peer_pad) );

    if ( (peer_element = gst_pad_get_parent_element (peer_pad) ) ) {
      peerElementName =
        debug_dump_make_object_name (GST_OBJECT (peer_element) );
    }

    /* pad link */
    ret += indentStr + elementName + "_" + padName + " -> " + peerElementName +
           "_" + peerPadName;

    if (!media.empty() ) {
      ret += " [label=\"" + media + "\"]\n";
    } else if (!mediaSrc.empty() && !mediaSink.empty() ) {
      /* dot has some issues with placement of head and taillabels,
       * we need an empty label to make space */
      ret += " [labeldistance=\"10\", labelangle=\"0\", "
             "label=\"                                                  \", "
             "taillabel=\"" + mediaSrc + "\", headlabel=\"" + mediaSink + "\"]\n";
    }

    if (peer_element) {
      gst_object_unref (peer_element);
    }

    gst_object_unref (peer_pad);
  }

  return ret;
}

static std::string
debug_dump_element (GstBin *bin, GstDebugGraphDetails details,
                    const gint indent)
{
  std::string ret;

  GstIterator *element_iter, *pad_iter;
  gboolean elements_done, pads_done;
  GValue item = { 0, };
  GValue item2 = { 0, };
  GstElement *element;
  GstPad *pad = NULL;
  guint src_pads, sink_pads;
  std::string elementName;
  std::string stateName;
  std::string params;
  std::string indentStr (indent * 2, ' ');

  element_iter = gst_bin_iterate_elements (bin);
  elements_done = FALSE;

  while (!elements_done) {
    switch (gst_iterator_next (element_iter, &item) ) {
    case GST_ITERATOR_OK:
      element = GST_ELEMENT (g_value_get_object (&item) );
      elementName = debug_dump_make_object_name (GST_OBJECT (element) );

      if (details & GST_DEBUG_GRAPH_SHOW_STATES) {
        stateName = debug_dump_get_element_state (GST_ELEMENT (element) );
      }

      if (details & GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS) {
        params = debug_dump_get_element_params (GST_ELEMENT (element) );
      }

      /* elements */
      ret += indentStr + "subgraph cluster_" + elementName + " {\n";
      ret += indentStr + "  fontname=\"Bitstream Vera Sans\";\n";
      ret += indentStr + "  fontsize=\"8\";\n";
      ret += indentStr + "  style=filled;\n";
      ret += indentStr + "  color=black;\n\n";
      ret += indentStr + "  label=\"" + std::string (G_OBJECT_TYPE_NAME (
               element) ) + "\\n" + std::string (GST_OBJECT_NAME (element) ) + stateName +
             params + "\";\n",

             src_pads = sink_pads = 0;

      if ( (pad_iter = gst_element_iterate_sink_pads (element) ) ) {
        ret += debug_dump_element_pads (pad_iter, pad, element, details, indent,
                                        &src_pads, &sink_pads);
        gst_iterator_free (pad_iter);
      }

      if ( (pad_iter = gst_element_iterate_src_pads (element) ) ) {
        ret += debug_dump_element_pads (pad_iter, pad, element, details, indent,
                                        &src_pads, &sink_pads);
        gst_iterator_free (pad_iter);
      }

      if (GST_IS_BIN (element) ) {
        ret += indentStr + "  fillcolor=\"#ffffff\";\n";
        /* recurse */
        ret += debug_dump_element (GST_BIN (element), details, indent + 1);
      } else {
        if (src_pads && !sink_pads) {
          ret += indentStr + "  fillcolor=\"#ffaaaa\";\n";
        } else if (!src_pads && sink_pads) {
          ret += indentStr + "  fillcolor=\"#aaaaff\";\n";
        } else if (src_pads && sink_pads) {
          ret += indentStr + "  fillcolor=\"#aaffaa\";\n";
        } else {
          ret += indentStr + "  fillcolor=\"#ffffff\";\n";
        }
      }

      ret += indentStr + "}\n\n";

      if ( (pad_iter = gst_element_iterate_pads (element) ) ) {
        pads_done = FALSE;

        while (!pads_done) {
          switch (gst_iterator_next (pad_iter, &item2) ) {
          case GST_ITERATOR_OK:
            pad = GST_PAD (g_value_get_object (&item2) );

            if (gst_pad_is_linked (pad) ) {
              if (gst_pad_get_direction (pad) == GST_PAD_SRC) {
                ret += debug_dump_element_pad_link (pad, element, details, indentStr);
              } else {
                GstPad *peer_pad = gst_pad_get_peer (pad);

                if (peer_pad) {
                  if (!GST_IS_GHOST_PAD (peer_pad)
                      && GST_IS_PROXY_PAD (peer_pad) ) {
                    ret += debug_dump_element_pad_link (peer_pad, NULL, details, indentStr);
                  }

                  gst_object_unref (peer_pad);
                }
              }
            }

            g_value_reset (&item2);
            break;

          case GST_ITERATOR_RESYNC:
            gst_iterator_resync (pad_iter);
            break;

          case GST_ITERATOR_ERROR:
          case GST_ITERATOR_DONE:
            pads_done = TRUE;
            break;
          }
        }

        g_value_unset (&item2);
        gst_iterator_free (pad_iter);
      }

      g_value_reset (&item);
      break;

    case GST_ITERATOR_RESYNC:
      gst_iterator_resync (element_iter);
      break;

    case GST_ITERATOR_ERROR:
    case GST_ITERATOR_DONE:
      elements_done = TRUE;
      break;
    }
  }

  g_value_unset (&item);
  gst_iterator_free (element_iter);

  return ret;
}

std::string
generateDotGraph (GstBin *bin, GstDebugGraphDetails details)
{
  std::string retString;

  g_return_val_if_fail (GST_IS_BIN (bin), "");

  std::string stateName;
  std::string params;

  if (details & GST_DEBUG_GRAPH_SHOW_STATES) {
    stateName = debug_dump_get_element_state (GST_ELEMENT (bin) );
  }

  if (details & GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS) {
    params = debug_dump_get_element_params (GST_ELEMENT (bin) );
  }

  /* write header */
  retString = (
                "digraph pipeline {\n"
                "  rankdir=LR;\n"
                "  fontname=\"sans\";\n"
                "  fontsize=\"10\";\n"
                "  labelloc=t;\n"
                "  nodesep=.1;\n"
                "  ranksep=.2;\n"
                "  label=\"<" + std::string (G_OBJECT_TYPE_NAME (bin) ) + ">\\n" + std::string (
                  GST_OBJECT_NAME (bin) ) + stateName + params + "\";\n"
                "  node [style=filled, shape=box, fontsize=\"9\", fontname=\"sans\", margin=\"0.0,0.0\"];\n"
                "  edge [labelfontsize=\"6\", fontsize=\"9\", fontname=\"monospace\"];\n"
                "  \n"
                "  legend [\n"
                "    pos=\"0,0!\",\n"
                "    margin=\"0.05,0.05\",\n"
                "    label=\"Legend\\lElement-States: [~] void-pending, [0] null, [-] ready, [=] paused, [>] playing\\lPad-Activation: [-] none, [>] push, [<] pull\\lPad-Flags: [b]locked, [f]lushing, [b]locking; upper-case is set\\lPad-Task: [T] has started task, [t] has paused task\\l\"\n"
                "  ];"
                "\n");

  retString += debug_dump_element (bin, details, 1);

  /* write footer */
  retString += "}\n";

  return retString;
}

} /* kurento */
