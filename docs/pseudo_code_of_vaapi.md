```cpp
// Initialization
dpy = vaGetDisplayDRM(fd);
vaInitialize(dpy, ...);
 
// Create surfaces required for decoding and subsequence encoding
vaCreateSurfaces(dpy, VA_RT_FORMAT_YUV420, width, height, &surfaces[0], ...);
 
// Set up a queue for the surfaces shared between decode and encode threads
surface_queue = queue_create();
 
// Create decode_thread
pthread_create(&decode_thread, NULL, decode, ...);
 
// Create encode_thread
pthread_create(&encode_thread, NULL, encode, ...);
 
// Decode thread function
decode() {
  // Find the decode entrypoint for H.264
  vaQueryConfigEntrypoints(dpy, h264_profile, entrypoints, ...);
 
  // Create a config for H.264 decode
  vaCreateConfig(dpy, h264_profile, VAEntrypointVLD, ...);
 
  // Create a context for decode
  vaCreateContext(dpy, config, width, height, VA_PROGRESSIVE, surfaces,
    num_surfaces, &decode_context);
 
  // Decode frames in the bitstream
  for (;;) {
    // Parse one frame and decode
    vaBeginPicture(dpy, decode_context, surfaces[surface_index]);
    vaRenderPicture(dpy, decode_context, buf, ...);
    vaEndPicture(dpy, decode_context);
    // Poll the decoding status and enqueue the surface in display order after
    // decoding is complete
    vaQuerySurfaceStatus();
    enqueue(surface_queue, surface_index);
  }
}
 
// Encode thread function
encode() {
  // Find the encode entrypoint for HEVC
  vaQueryConfigEntrypoints(dpy, hevc_profile, entrypoints, ...);
 
  // Create a config for HEVC encode
  vaCreateConfig(dpy, hevc_profile, VAEntrypointEncSlice, ...);
 
  // Create a context for encode
  vaCreateContext(dpy, config, width, height, VA_PROGRESSIVE, surfaces,
    num_surfaces, &encode_context);
 
  // Encode frames produced by the decoder
  for (;;) {
    // Dequeue the surface enqueued by the decoder
    surface_index = dequeue(surface_queue);
    // Encode using this surface as the source
    vaBeginPicture(dpy, encode_context, surfaces[surface_index]);
    vaRenderPicture(dpy, encode_context, buf, ...);
    vaEndPicture(dpy, encode_context);
  }
}
```