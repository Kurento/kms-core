6.6.1
=====

  * Fix problem in pts synchronization algorithm when remote is sending
    wrong RTCP SR packages that produces backwards PTS.
  * Improve thread management when using filters. Filter processing uses
    its own thread and drops packages that are late.
  * Improve compilation issues.

6.6.0
=====

  * Bugs fixes:

     * Improve rtp synchronization algorithm, this makes recorder behave
       better when recording from RtpEndpoint or WebRTCEndpoint
     * Fix memory problems during Media Elements disconnections. This problem
       was also present when
     * Improve Media Elements connection, some cases were not working
       correctly, specially when creating multi stream elements
     * Fix memory problems in flowOut/flowIn events detection
     * Allow c++ to listen to signals with at return value
     * Memory leaks
     * Fix some rare media deadlocks on on AgnosticBin
     * UriEndpoint: Add support
  * Update SDP Agent

     * Make code cleaner
     * Add support for error notification using GError, this allows
       raising betters exceptions to client
  * Documentation improvements
  * ServerManager: Add method to get memory used by the server
  * UriEndpoint: Add property to get state
  * UriEndpoint: Add event to notify state changes

6.5.0
=====

  * Change license to Apache 2.0
  * Fix bugs in Flow IN - Flow OUT event (caused a segmentation fault)
  * REMB algorithm improvements
  * Fix max/min video bandwidth parameters (now 0 means unlimited)
  * Improve the API changing some event/methods names and deprecating old ones
    (even they are still available, it's recommended to not use them as they
     can be remoted on the next major release)
  * Documentation inprovement
  * Raise events from differents threads
  * Agnosticbin: Add support for rtp format (only at output)

6.4.0
=====

  * Prepare implementation to support multistream
  * Fix bad timestamp for opus codec
  * Improve latency stats to add support for multiple streams
  * Fix latency stats calculation
  * Add flow in and flow out signals that indicates if there is media
    going in or out to/from a media element
  * Some fixes in SDP Agent
  * Remb management improvements
  * Add leaky queue in filters to avoid them to buffer media if the proccess
    slower than buffers arrive

6.3.1
=====

  * Fix problem with codecs written in lower/upper case
  * Minor code improvements

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
