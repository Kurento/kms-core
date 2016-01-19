6.3.0
=====

  * SdpEndpoint: Add support for negotiating ipv6 or ipv4
  * Update glib to 2.46
  * SdpEndpoint: Fix bug on missordered medias when they are bunble
  * Add compilation time to module information (makes debugging easier)
  * KurentoException: Add excetions for player
  * agnosticbin: Fix many negotiation problems caused by new empty caps
  * MediaElement/MediaPipeline: Fix segmentation fault when error event is sent
  * agnosticbin: Do not negotiate pad until a reconfigure event is received
    (triying to do so can cause deadlock)
  * sdpAgent: Support mid without a group (Fixes problems with Firefox)
  * Fix problem with REMB notifications when we are sending too much nack events

6.2.0
=====

  * Update GStreamer version to 1.7
  * RtpEndpoint: Add address in generated SDP. 0.0.0.0 was added so no media
    could return to the server.
  * SdpEndpoint: Add maxAudioRecvBandwidth property
  * BaseRtpEndpoint: Add configuration for port ranges
  * Fixed https://github.com/Kurento/bugtracker/issues/12
  * SdpEndpoint: Raises error when sdp answer cannot be processed
  * MediaPipeline: Add proper error code to error events
  * MediaElement: Fix error notification mechanisms. Errors where not raising in
    most cases
  * UriEndpoint: Add default uri for relative paths. Now uris without schema are
    treated with a default value set in configuration. By default directory
    /var/kurento is used
  * Improvements in format negotiations between elements, this is fixing
    problems in:
    * RecorderEndpoint
    * Composite
  * RecorderEndpoint: Change audio format for WEBM from Vorbis to Opus. This is
    avoiding transcodification and also improving quality.
