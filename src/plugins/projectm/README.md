# projectM visualizer

The plugin uses SDL2 and Dear ImGui, linked into the plugin. It opens a
separate OpenGL window on Windows, macOS, X11, or Wayland. Windows hosts need
an OpenGL 3.3 driver. Linux hosts need an OpenGL driver and the libraries for
their desktop session. On macOS, a bundled helper owns the window and receives
audio from the plugin.

The bundled presets live in the `projectM` directory beside the plugin and are
loaded automatically. To use another collection of `.milk` presets, enter its
directory in the controls and select **Load presets**, or set
`MUSIKCUBE_PROJECTM_PRESETS` before starting the application.

The plugin saves its selected preset, lock state, custom collection directory,
audio gain, duration, transition-work option, fullscreen state, window size, and
controls visibility in musikcore's plugin settings store. Changes are saved as
they happen and restored when the window or application opens again. Presets
are remembered by relative path, so adding files does not change the selection.
If the previous preset is missing, the first available preset is used.
`MUSIKCUBE_PROJECTM_PRESETS` takes precedence over the saved directory.

The toolbar fades when the pointer is idle and returns when it moves. Controls
stay visible while a panel is open. Use Find to search preset names, or Settings
to change the directory, gain, and duration.

Use the left and right arrow keys to change presets, Space for a random preset,
F for fullscreen, and Tab to hide or show the controls. Escape closes a visible
panel first, then asks for confirmation when no panel is visible. Choose Yes to
close the window (Enter or Y), or choose No (N or Escape) to cancel. The controls
also adjust the automatic advance interval.

The Linux standalone vendor build requires a host C compiler and Expat
development files to build its Wayland protocol generator.
