# run any given test by running make test.check
# if the test fails, run it again at at least debug level 2
%.check: %
	@$(TESTS_ENVIRONMENT)					\
	CK_DEFAULT_TIMEOUT=20					\
	$* ||							\
	$(TESTS_ENVIRONMENT)					\
	GST_DEBUG=$$GST_DEBUG,*:3				\
	CK_DEFAULT_TIMEOUT=20					\
	$*

CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.reg

REGISTRY_ENVIRONMENT =						\
	GST_REGISTRY=$(CHECK_REGISTRY)

TESTS_ENVIRONMENT =						\
	CK_DEFAULT_TIMEOUT=30					\
	$(REGISTRY_ENVIRONMENT)

CLEANFILES = core.* test-registry.*

$(CHECK_REGISTRY):
	$(TESTS_ENVIRONMENT)

TESTS =	$(check_PROGRAMS)

check_PROGRAMS =						\
	general/dummytest

general_dummytest_CFLAGS = $(GST_CFLAGS) $(GST_CHECK_CFLAGS) -Werror
general_dummytest_LDADD = $(GST_LIBS) $(GST_CHECK_LIBS)
