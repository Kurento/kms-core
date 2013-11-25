
if HAS_SOUP

noinst_PROGRAMS = webrtc_http_server

webrtc_http_server_SOURCES = webrtc_http_server.c \
		$(top_builddir)/tests/check/kmstestutils.c
webrtc_http_server_CFLAGS = $(GST_CFLAGS) $(SOUP_CFLAGS) $(NICE_CFLAGS) -Werror \
		-I $(top_builddir)/tests/check
webrtc_http_server_LDADD = $(GST_LIBS) $(SOUP_LIBS) $(NICE_LIBS)
endif
