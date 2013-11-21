
if HAS_SOUP
noinst_PROGRAMS = webrtc_http_server

webrtc_http_server_SOURCES = webrtc_http_server.c
webrtc_http_server_CFLAGS = $(GST_CFLAGS) $(SOUP_CFLAGS) $(NICE_CFLAGS) -Werror
webrtc_http_server_LDADD = $(GST_LIBS) $(SOUP_LIBS) $(NICE_LIBS)
endif
