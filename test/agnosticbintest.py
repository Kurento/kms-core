#!/usr/bin/python3

import sys
import gi

gi.require_version('Gst', '1.0')
from gi.repository import Gst
from gi.repository import GLib

loop = GLib.MainLoop()

def disconnect_videosink(pipe):
  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL,
      "removevideosink0")
  videosink = pipe.get_by_name("videosink")
  pipe.remove(videosink);
  videosink.set_state(Gst.State.NULL);
  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL,
      "removevideosink1")
  return False

def connect_videosink(pipe):
  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "videosink0")
  videosink = Gst.ElementFactory.make("xvimagesink", "videosink")
  videosink.set_state(Gst.State.PLAYING)
  pipe.add(videosink)
  agnostic = pipe.get_by_name("agnostic")

  agnostic.link(videosink)

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "videosink1")
  return False

def connect_audiosink(pipe):
  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "audiosink0")
  audiosink = Gst.ElementFactory.make("autoaudiosink", None)
  audiosink.set_state(Gst.State.PLAYING)
  pipe.add(audiosink)
  agnostic = pipe.get_by_name("agnostic")

  agnostic.link(audiosink)

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "audiosink1")
  return False

def main(argv):
  Gst.init(argv)

  pipe = Gst.Pipeline()

  videotest = Gst.ElementFactory.make("videotestsrc", None)
  agnostic = Gst.ElementFactory.make("agnosticbin", "agnostic")
  videosink = Gst.ElementFactory.make("xvimagesink", None)

  videotest.set_property("pattern", "ball")

  pipe.add(videotest)
  pipe.add(agnostic)
  pipe.add(videosink)

  videotest.link(agnostic)
  agnostic.link(videosink)
  pipe.set_state(Gst.State.PLAYING)

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "playing")
  GLib.timeout_add_seconds(2, connect_videosink, pipe)
  GLib.timeout_add_seconds(4, connect_audiosink, pipe)
  GLib.timeout_add_seconds(5, disconnect_videosink, pipe)

  try:
    loop.run()
  except:
    pass

  Gst.debug_bin_to_dot_file_with_ts(pipe, Gst.DebugGraphDetails.ALL, "end")
  pipe.set_state(Gst.State.NULL)

if __name__ == "__main__":
  main(sys.argv)
