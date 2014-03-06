CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.reg

REGISTRY_ENVIRONMENT =						\
	GST_REGISTRY=$(CHECK_REGISTRY)				\
	GST_PLUGIN_PATH=$(top_builddir):$(GST_PLUGINS_DIR)

TESTS_ENVIRONMENT =						\
	CK_DEFAULT_TIMEOUT=20					\
	G_SLICE=always-malloc 					\
	MALLOC_CHECK=1						\
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

check_PROGRAMS =						\
	$(TEST_AUTOMUXER)					\
        $(TEST_INTEGRATION)                                     \
	element/rtpendpoint					\
	element/rtpendpoint_audio				\
	element/rtpendpoint_video				\
	element/webrtcendpoint					\
	general/dummytest					\
	general/sdp						\
	general/utils						\
	general/basehub						\
	element/mixerport					\
	element/filterelement					\
	element/uriendpoint					\
	element/recorderendpoint				\
	element/playerendpoint                                  \
	element/httpendpoint					\
	element/agnosticbin					\
	element/sctp                                            \
	element/mainmixer                                       \
	memory_leaks/agnosticbin				\
	memory_leaks/playerendpoint				\
	memory_leaks/recorderendpoint

lib_LTLIBRARIES = libkms_test_utils.la
libkms_test_utils_la_SOURCES = kmstestutils.h kmstestutils.c
libkms_test_utils_la_CFLAGS = $(GST_CFLAGS) -Werror
libkms_test_utils_la_LIBADD = $(GST_LIBS)

element_rtpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la

element_rtpendpoint_audio_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_audio_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la

element_rtpendpoint_video_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_rtpendpoint_video_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la
if DEBUGGING_TESTS
element_rtpendpoint_video_CFLAGS += -DDEBUGGING_TESTS
endif
if DEBUGGING_TESTS
element_rtpendpoint_video_CFLAGS += -DDEBUGGING_TESTS
endif

element_webrtcendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_webrtcendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la

general_dummytest_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
general_dummytest_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

general_basehub_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
general_basehub_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/src/libgstkurento_la-kmsbasehub.lo \
		$(top_builddir)/src/libgstkurento_la-kmsutils.lo \
		$(top_builddir)/src/libgstkurento_la-kmselement.lo \
		$(top_builddir)/src/libgstkurento_la-kmsmixerport.lo \
		$(top_builddir)/src/libgstkurento_la-kms-marshal.lo

general_sdp_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
general_sdp_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/src/libgstkurento_la-sdp_utils.lo

general_utils_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
general_utils_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/src/libgstkurento_la-kmsutils.lo

element_mixerport_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
		-I$(top_srcdir)/src
element_mixerport_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

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
element_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la

element_httpendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
element_httpendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_agnosticbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
element_agnosticbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
if DEBUGGING_TESTS
element_agnosticbin_CFLAGS += -DDEBUGGING_TESTS
endif

element_sctp_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
element_sctp_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

element_mainmixer_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
element_mainmixer_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

# Integration tests
if INTEGRATIONTESTS
integration_playerendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror \
                -I$(top_srcdir)/src
integration_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS) \
		$(top_builddir)/tests/check/libkms_test_utils.la
endif

# Memory leaks tests

memory_leaks_agnosticbin_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
memory_leaks_agnosticbin_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

memory_leaks_playerendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -I$(top_srcdir)/src -Werror
memory_leaks_playerendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

memory_leaks_recorderendpoint_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -I$(top_srcdir)/src -Werror
memory_leaks_recorderendpoint_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)

