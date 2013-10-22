CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.reg

REGISTRY_ENVIRONMENT =						\
	GST_REGISTRY=$(CHECK_REGISTRY)				\
	GST_PLUGIN_PATH=$(top_builddir)/src:$(GST_PLUGINS_DIR)

TESTS_ENVIRONMENT =						\
	CK_DEFAULT_TIMEOUT=20					\
	$(REGISTRY_ENVIRONMENT)

# run any given test by running make test.check
# if the test fails, run it again at at least debug level 2
%.check: %
	@$(TESTS_ENVIRONMENT)					\
	$*

CLEANFILES = core.* test-registry.*

$(CHECK_REGISTRY):
	$(TESTS_ENVIRONMENT)

TESTS =	$(check_PROGRAMS)

if INTEGRATIONTESTS
TEST_INTEGRATION = integration/playerendpoint
else
TEST_INTEGRATION =
endif

if AUTOMUXERTEST
TEST_AUTOMUXER = element/automuxerbin
else
TEST_AUTOMUXER =
endif

if AGNOSTICBINTEST
TEST_AGNOSTICBIN = element/agnosticbin				\
	memory_leaks/agnosticbin
else
TEST_AGNOSTICBIN =
endif

check_PROGRAMS =						\
	$(TEST_AUTOMUXER)					\
	$(TEST_AGNOSTICBIN)					\
        $(TEST_INTEGRATION)                                     \
	element/rtpendpoint					\
	element/rtpendpoint_audio				\
	element/rtpendpoint_video				\
	general/dummytest					\
	general/sdp						\
	element/filterelement					\
	element/uriendpoint					\
	element/recorderendpoint				\
	element/playerendpoint                                  \
	element/httpendpoint					\
	element/agnosticbin2					\
	memory_leaks/agnosticbin2				\
	memory_leaks/playerendpoint				\
	memory_leaks/recorderendpoint

element_rtpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_rtpendpoint_audio_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_audio_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_rtpendpoint_video_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_video_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
if DEBUGGING_TESTS
element_rtpendpoint_video_CFLAGS += -DDEBUGGING_TESTS
endif
if DEBUGGING_TESTS
element_rtpendpoint_video_CFLAGS += -DDEBUGGING_TESTS
endif

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

if AGNOSTICBINTEST
element_agnosticbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_agnosticbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
if DEBUGGING_TESTS
element_agnosticbin_CFLAGS += -DDEBUGGING_TESTS
endif
endif

element_agnosticbin2_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_agnosticbin2_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
if DEBUGGING_TESTS
element_agnosticbin2_CFLAGS += -DDEBUGGING_TESTS
endif

# Integration tests
if INTEGRATIONTESTS
integration_playerendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
integration_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
endif

# Memory leaks tests

if AGNOSTICBINTEST
memory_leaks_agnosticbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
memory_leaks_agnosticbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
endif

memory_leaks_agnosticbin2_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
memory_leaks_agnosticbin2_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

memory_leaks_playerendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -I$(top_srcdir)/src -Werror
memory_leaks_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

memory_leaks_recorderendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -I$(top_srcdir)/src -Werror
memory_leaks_recorderendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

