#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

static Suite *
recorderendpoint_suite (void)
{
  Suite *s = suite_create ("recorderendpoint");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (recorderendpoint);
