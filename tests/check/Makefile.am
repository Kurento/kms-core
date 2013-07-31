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

check_PROGRAMS =						\
	element/rtpendpoint					\
	general/dummytest					\
	general/sdp						\
	element/filterelement					\
	element/automuxerbin					\
        element/uriendpoint					\
	element/recorderendpoint

element_rtpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

general_dummytest_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
general_dummytest_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

general_sdp_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
general_sdp_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/src/libgstkurento_la-sdp_utils.lo

element_automuxerbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_automuxerbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_filterelement_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_filterelement_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_uriendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
element_uriendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_recorderendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_recorderendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

