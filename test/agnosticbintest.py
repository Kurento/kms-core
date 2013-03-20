#!/usr/bin/python3

import sys
import gi

gi.require_version('Gst', '1.0')
from gi.repository import Gst
from gi.repository import GLib

loop = GLib.MainLoop()

def main(argv):
  Gst.init(argv)

  pipe = Gst.Pipeline()

  videotest = Gst.ElementFactory.make("videotestsrc", None)
  agnostic = Gst.ElementFactory.make("agnosticbin", None)

  pipe.add(videotest)
  pipe.add(agnostic)

  videotest.link(agnostic)
  pipe.set_state(Gst.State.PLAYING)

  try:
    loop.run()
  except:
    pass

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "end")
  pipe.set_state(Gst.State.NULL)


if __name__ == "__main__":
  main(sys.argv)
