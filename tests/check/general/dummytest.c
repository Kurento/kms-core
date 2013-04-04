#include <gst/check/gstcheck.h>

static void
setup (void)
{
  GST_INFO ("Do some startup here");
}

static void
teardown (void)
{
  GST_INFO ("Do cleaup here");
}

GST_START_TEST (dummy1)
{
  GST_INFO ("Perform test1 here");
  fail_if (FALSE);
}

GST_END_TEST
GST_START_TEST (dummy2)
{
  GST_INFO ("Perform test2 here");
  fail_if (FALSE);
}

GST_END_TEST
/*
 * End of test cases
 */
static Suite *
dummy_suite (void)
{
  Suite *s = suite_create ("dummy");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, dummy1);
  tcase_add_test (tc_chain, dummy2);

  return s;
}

GST_CHECK_MAIN (dummy);
