CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.reg

REGISTRY_ENVIRONMENT =						\
	GST_REGISTRY=$(CHECK_REGISTRY)				\
	GST_PLUGIN_PATH=$(top_builddir)/src:$(GST_PLUGINS_DIR)

TESTS_ENVIRONMENT =						\
	CK_DEFAULT_TIMEOUT=60					\
	$(REGISTRY_ENVIRONMENT)

# run any given test by running make test.check
# if the test fails, run it again at at least debug level 2
%.check: %
	@$(TESTS_ENVIRONMENT)					\
	$* ||							\
	$(TESTS_ENVIRONMENT)					\
	GST_DEBUG=*:3,$$GST_DEBUG				\
	$*

CLEANFILES = core.* test-registry.*

$(CHECK_REGISTRY):
	$(TESTS_ENVIRONMENT)

TESTS =	$(check_PROGRAMS)

if AUTOMUXERTEST
TEST_AUTOMUXER = element/automuxerbin
else
TEST_AUTOMUXER =
endif

check_PROGRAMS =						\
	$(TEST_AUTOMUXER)					\
	element/rtpendpoint					\
	element/rtpendpoint_video				\
	general/dummytest					\
	general/sdp						\
	element/filterelement					\
	element/uriendpoint					\
	element/recorderendpoint				\
	element/playerendpoint                                  \
        element/httpendpoint					\
        element/agnosticbin

element_rtpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_rtpendpoint_video_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_video_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

general_dummytest_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
general_dummytest_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

general_sdp_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
general_sdp_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/src/libgstkurento_la-sdp_utils.lo

element_filterelement_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_filterelement_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

if AUTOMUXERTEST
element_automuxerbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_automuxerbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
endif

element_uriendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
element_uriendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_recorderendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
element_recorderendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_playerendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
element_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_httpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
element_httpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_agnosticbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_agnosticbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
