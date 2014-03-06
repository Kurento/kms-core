/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#include <gst/gst.h>
#include <libsoup/soup.h>
#include <nice/nice.h>
#include <string.h>
#include <gst/sdp/gstsdpmessage.h>

#include <kmstestutils.h>

#define GST_CAT_DEFAULT webrtc_http_server
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEBUG_NAME "webrtc_http_server"

#define PORT 8080
#define MIME_TYPE "text/html"
#define CLIENT_ANSWERER_HTML_FILE "webrtc_loopback.html"
#define CLIENT_OFFERER_HTML_FILE "webrtc_loopback_offerer.html"

#define WEBRTC_ENDPOINT "webrtc-end-point"
#define MEDIA_SESSION_ID "media-session-id"
#define FILTER_TYPE "filter-type"

static GMainLoop *loop;
static GRand *rand;
static GHashTable *cookies;

static const gchar *pattern_sdp_vp8_sendrecv_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=0 0\r\n"
    "m=audio 0 RTP/AVP 0 96\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendrecv\r\n"
    "a=rtpmap:96 OPUS/48000/1\r\n" "a=sendrecv\r\n"
    "m=video 0 RTP/AVP 97\r\n" "a=rtpmap:97 VP8/90000\r\n" "a=sendrecv\r\n";

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      gint64 *id;
      gchar *error_file;

      error_file = g_strdup_printf ("error-%s", GST_OBJECT_NAME (pipe));
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, error_file);
      g_free (error_file);

      id = g_object_get_data (pipe, MEDIA_SESSION_ID);
      if (id != NULL) {
        /* This destroys the pipeline */
        g_hash_table_remove (cookies, id);
      }

      /* TODO: free mediaSession */
      break;
    }
    case GST_MESSAGE_WARNING:{
      gchar *warn_file = g_strdup_printf ("warning-%s", GST_OBJECT_NAME (pipe));

      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, warn_file);
      g_free (warn_file);
      break;
    }
    default:
      break;
  }
}

static void
configure_media_session (GstElement * pipe)
{
  GstElement *webrtcendpoint =
      g_object_get_data (G_OBJECT (pipe), WEBRTC_ENDPOINT);
  const gchar *filter_type =
      g_object_get_data (G_OBJECT (webrtcendpoint), FILTER_TYPE);
  GstElement *audio_identity = gst_element_factory_make ("identity", NULL);

  GST_INFO ("Filter type: %s", filter_type);

  if (g_strcmp0 (filter_type, "clockoverlay") == 0) {
    GstElement *agnostic = gst_element_factory_make ("agnosticbin", NULL);
    GstElement *clockoverlay = gst_element_factory_make ("clockoverlay", NULL);

    g_object_set (clockoverlay, "font-desc", "Sans 28", NULL);

    gst_bin_add_many (GST_BIN (pipe), agnostic, clockoverlay, NULL);
    gst_element_sync_state_with_parent (clockoverlay);
    gst_element_sync_state_with_parent (agnostic);

    kms_element_link_pads (webrtcendpoint, "video_src_%u", clockoverlay,
        "video_sink");
    gst_element_link (clockoverlay, agnostic);
    gst_element_link_pads (agnostic, NULL, webrtcendpoint, "video_sink");
  } else if (g_strcmp0 (filter_type, "jackvader") == 0) {
    GstElement *agnostic = gst_element_factory_make ("agnosticbin", NULL);
    GstElement *jackvader = gst_element_factory_make ("filterelement", NULL);

    g_object_set (jackvader, "filter-factory", "jackvader", NULL);

    gst_bin_add_many (GST_BIN (pipe), agnostic, jackvader, NULL);
    gst_element_sync_state_with_parent (jackvader);
    gst_element_sync_state_with_parent (agnostic);

    kms_element_link_pads (webrtcendpoint, "video_src_%u", jackvader,
        "video_sink");
    gst_element_link_pads (jackvader, "video_src_%u", agnostic, NULL);
    gst_element_link_pads (agnostic, NULL, webrtcendpoint, "video_sink");
  } else {
    GstElement *video_identity = gst_element_factory_make ("identity", NULL);

    gst_bin_add (GST_BIN (pipe), video_identity);
    gst_element_sync_state_with_parent (video_identity);

    kms_element_link_pads (webrtcendpoint, "video_src_%u", video_identity,
        "sink");
    gst_element_link_pads (video_identity, NULL, webrtcendpoint, "video_sink");
  }

  gst_bin_add (GST_BIN (pipe), audio_identity);
  gst_element_sync_state_with_parent (audio_identity);
  kms_element_link_pads (webrtcendpoint, "audio_src_%u", audio_identity,
      "sink");
  gst_element_link_pads (audio_identity, NULL, webrtcendpoint, "audio_sink");
}

static void
process_sdp_answer (GstElement * pipe, const gchar * sdp_str)
{
  GstSDPMessage *sdp = NULL;
  GstElement *webrtcendpoint =
      g_object_get_data (G_OBJECT (pipe), WEBRTC_ENDPOINT);

  GST_DEBUG ("Process SDP answer:\n%s", sdp_str);

  gst_sdp_message_new (&sdp);
  gst_sdp_message_parse_buffer ((const guint8 *) sdp_str, -1, sdp);
  g_signal_emit_by_name (webrtcendpoint, "process-answer", sdp);
  gst_sdp_message_free (sdp);

  configure_media_session (pipe);
}

static GstElement *
init_media_session (SoupServer * server, SoupMessage * msg, gint64 id,
    const gchar * client_offer)
{
  gint64 *id_pointer;
  GstSDPMessage *sdp;
  GstElement *pipe = gst_pipeline_new (NULL);
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  GstElement *webrtcendpoint =
      gst_element_factory_make ("webrtcendpoint", NULL);
  gchar *sdp_str = NULL;

  g_object_set (G_OBJECT (pipe), "async-handling", TRUE, NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipe);
  g_object_unref (bus);

  id_pointer = g_malloc (sizeof (guint64));
  *id_pointer = id;
  g_object_set_data_full (G_OBJECT (pipe), MEDIA_SESSION_ID, id_pointer,
      g_free);
  g_object_set_data_full (G_OBJECT (pipe), WEBRTC_ENDPOINT,
      g_object_ref (webrtcendpoint), g_object_unref);

  gst_sdp_message_new (&sdp);
  gst_sdp_message_parse_buffer ((const guint8 *) pattern_sdp_vp8_sendrecv_str,
      -1, sdp);
  g_object_set (webrtcendpoint, "pattern-sdp", sdp, NULL);
  gst_sdp_message_free (sdp);

  gst_bin_add (GST_BIN (pipe), webrtcendpoint);

  if (client_offer != NULL) {
    GstSDPMessage *sdp = NULL, *asnwer;

    GST_DEBUG ("Process SDP offer:\n%s", client_offer);

    gst_sdp_message_new (&sdp);
    gst_sdp_message_parse_buffer ((const guint8 *) client_offer, -1, sdp);
    g_signal_emit_by_name (webrtcendpoint, "process-offer", sdp, &asnwer);
    gst_sdp_message_free (sdp);

    sdp_str = gst_sdp_message_as_text (asnwer);
    gst_sdp_message_free (asnwer);
    GST_DEBUG ("Answer:\n%s", sdp_str);
    g_free (sdp_str);

    configure_media_session (pipe);
  } else {
    g_signal_emit_by_name (webrtcendpoint, "generate-offer", &sdp);
    sdp_str = gst_sdp_message_as_text (sdp);
    gst_sdp_message_free (sdp);
    GST_DEBUG ("Offer:\n%s", sdp_str);
    g_free (sdp_str);
  }

  gst_element_set_state (pipe, GST_STATE_PLAYING);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
      GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME (pipe));

  return pipe;
}

static void
server_callback (SoupServer * server, SoupMessage * msg, const char *path,
    GHashTable * query, SoupClientContext * client, gpointer user_data)
{
  gboolean ret;
  gchar *contents;
  gsize length;
  const char *cookie_str;
  SoupCookie *cookie = NULL;
  gint64 id, *id_ptr;
  GRegex *regex;
  gchar *line, *sdp_str;
  char *header;
  GstElement *pipe = NULL;
  GstElement *webrtcendpoint;
  GstSDPMessage *local_sdp;
  gchar *local_sdp_str;
  gchar *filter_type = NULL;
  gboolean client_offerer = FALSE;
  const gchar *html_file;

  GST_DEBUG ("Request: %s", path);

  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    GST_DEBUG ("Not implemented");
    return;
  }

  if (g_strcmp0 (path, "/") == 0) {
    client_offerer = FALSE;
    html_file = CLIENT_ANSWERER_HTML_FILE;
  } else if (g_strcmp0 (path, "/offerer") == 0) {
    client_offerer = TRUE;
    html_file = CLIENT_OFFERER_HTML_FILE;
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    GST_DEBUG ("Not found");
    return;
  }

  ret = g_file_get_contents (html_file, &contents, &length, NULL);
  if (!ret) {
    GST_ERROR ("Error loading %s file", html_file);
    soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
    return;
  }

  soup_message_set_response (msg, MIME_TYPE, SOUP_MEMORY_STATIC, "", 0);

  cookie_str = soup_message_headers_get_list (msg->request_headers, "Cookie");
  if (cookie_str != NULL) {
    gchar **tokens, **token;

    tokens = g_strsplit (cookie_str, ";", 0);
    for (token = tokens; *token != NULL; token++) {
      cookie = soup_cookie_parse (*token, NULL);

      if (cookie != NULL) {
        if (g_strcmp0 (cookie->name, "id") == 0) {
          id = g_ascii_strtoll (cookie->value, NULL, 0);
          if (id != G_GINT64_CONSTANT (0)) {
            GST_DEBUG ("Found id: %" G_GINT64_FORMAT, id);
            pipe = g_hash_table_lookup (cookies, &id);
            break;
          }
        }
        soup_cookie_free (cookie);
        cookie = NULL;
      }
    }
    g_strfreev (tokens);
  }

  if (cookie == NULL) {
    gchar *id_str;
    const gchar *host;

    id = g_rand_double_range (rand, (double) G_MININT64, (double) G_MAXINT64);
    id_str = g_strdup_printf ("%" G_GINT64_FORMAT, id);
    host = soup_message_headers_get_one (msg->request_headers, "Host");
    if (host == NULL) {
      host = "localhost";
    }

    cookie = soup_cookie_new ("id", id_str, host, path, -1);
    g_free (id_str);
  }

  if (query != NULL) {
    gchar *sdp = g_hash_table_lookup (query, "sdp");

    filter_type = g_strdup (g_hash_table_lookup (query, FILTER_TYPE));

    if (client_offerer) {
      if (sdp == NULL)
        return;

      /* TODO: reuse source */
      pipe = init_media_session (server, msg, id, sdp);
      id_ptr = g_malloc (sizeof (gint64));
      *id_ptr = id;
      g_hash_table_insert (cookies, id_ptr, pipe);

      webrtcendpoint = g_object_get_data (G_OBJECT (pipe), WEBRTC_ENDPOINT);
      g_object_set_data_full (G_OBJECT (webrtcendpoint), FILTER_TYPE,
          filter_type, g_free);

      g_object_get (webrtcendpoint, "local-answer-sdp", &local_sdp, NULL);
      local_sdp_str = gst_sdp_message_as_text (local_sdp);
      gst_sdp_message_free (local_sdp);

      soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE,
          local_sdp_str, strlen (local_sdp_str));

      soup_message_set_status (msg, SOUP_STATUS_OK);
      return;
    }

    if (sdp != NULL && pipe != NULL) {
      process_sdp_answer (pipe, sdp);
      soup_message_set_status (msg, SOUP_STATUS_OK);
      return;
    }
  }

  g_hash_table_remove (cookies, &id);

  header = soup_cookie_to_cookie_header (cookie);
  if (header != NULL) {
    soup_message_headers_append (msg->response_headers, "Set-Cookie", header);
    g_free (header);
  } else {
    GST_WARNING ("Null cookie");
  }
  soup_cookie_free (cookie);

  soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE, contents,
      length);

  if (client_offerer) {
    soup_message_set_status (msg, SOUP_STATUS_OK);
    return;
  }

  pipe = init_media_session (server, msg, id, NULL);
  id_ptr = g_malloc (sizeof (gint64));
  *id_ptr = id;
  g_hash_table_insert (cookies, id_ptr, pipe);

  webrtcendpoint = g_object_get_data (G_OBJECT (pipe), WEBRTC_ENDPOINT);
  g_object_set_data_full (G_OBJECT (webrtcendpoint), FILTER_TYPE, filter_type,
      g_free);

  g_object_get (webrtcendpoint, "local-offer-sdp", &local_sdp, NULL);
  local_sdp_str = gst_sdp_message_as_text (local_sdp);
  gst_sdp_message_free (local_sdp);

  regex = g_regex_new ("\r\n", G_REGEX_DOTALL | G_REGEX_OPTIMIZE, 0, NULL);
  sdp_str = g_regex_replace (regex,
      local_sdp_str, -1, 0, "\\\\r\\\\n\" +\n\"", 0, NULL);
  g_regex_unref (regex);
  g_free (local_sdp_str);

  line = g_strdup_printf ("sdp = \"%s\";\n", sdp_str);
  g_free (sdp_str);
  soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE, line,
      strlen (line));

  line = "</script>\n</body>\n</html>\n";
  soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC, line,
      strlen (line));

  soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void
destroy_pipe (gpointer data)
{
  GstElement *pipe = GST_ELEMENT (data);

  GST_DEBUG_OBJECT (pipe, "Destroy pipe");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe), GST_DEBUG_GRAPH_SHOW_ALL,
      GST_OBJECT_NAME (pipe));
  gst_element_set_state (pipe, GST_STATE_NULL);
  g_object_unref (pipe);
}

static void
sigHandler (int signal)
{
  g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
  SoupServer *server;

  gst_init (&argc, &argv);
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, DEBUG_NAME, 0, DEBUG_NAME);

  loop = g_main_loop_new (NULL, TRUE);
  cookies =
      g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, destroy_pipe);
  rand = g_rand_new ();

  signal (SIGINT, sigHandler);
  signal (SIGKILL, sigHandler);
  signal (SIGTERM, sigHandler);

  GST_INFO ("Start Kurento WebRTC HTTP server");
  server = soup_server_new (SOUP_SERVER_PORT, PORT, NULL);
  soup_server_add_handler (server, "/", server_callback, NULL, NULL);
  soup_server_run_async (server);

  if (g_main_loop_is_running (loop))
    g_main_loop_run (loop);

  g_hash_table_destroy (cookies);
  g_rand_free (rand);

  return 0;
}
