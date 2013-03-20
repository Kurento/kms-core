#!/usr/bin/python3

import sys
import gi

gi.require_version('Gst', '1.0')
from gi.repository import Gst
from gi.repository import GLib

loop = GLib.MainLoop()

def connect_videotest(pipe):
  videosink = pipe.get_by_name("videosink")
  agnostic = pipe.get_by_name("agnostic")

  agnostic.link(videosink)

  return False

def main(argv):
  Gst.init(argv)

  pipe = Gst.Pipeline()

  videotest = Gst.ElementFactory.make("videotestsrc", None)
  agnostic = Gst.ElementFactory.make("agnosticbin", "agnostic")
  videosink = Gst.ElementFactory.make("autovideosink", "videosink")
  fakesink = Gst.ElementFactory.make("fakesink", None)

  fakesink.set_property("sync", True);

  pipe.add(videotest)
  pipe.add(agnostic)
  pipe.add(videosink)
  pipe.add(fakesink)

  videotest.link(agnostic)
  agnostic.link(fakesink);
  pipe.set_state(Gst.State.PLAYING)

  GLib.timeout_add_seconds(5, connect_videotest, pipe)

  try:
    loop.run()
  except:
    pass

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "end")
  pipe.set_state(Gst.State.NULL)

if __name__ == "__main__":
  main(sys.argv)
