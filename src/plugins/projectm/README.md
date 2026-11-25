# projectM visualizer

The Linux plugin uses SDL2 and Dear ImGui, linked into the plugin. It opens a
separate OpenGL window on X11 or Wayland. The host needs an OpenGL driver and
the libraries for its desktop session.

The bundled presets live in the `projectM` directory beside the plugin and are
loaded automatically. To use another collection of `.milk` presets, enter its
directory in the controls and select **Load presets**, or set
`MUSIKCUBE_PROJECTM_PRESETS` before starting the application.

Use the left and right arrow keys to change presets, Space to toggle automatic
advance, F for fullscreen, Tab to hide or show the controls, and Escape to
close the window. The controls also adjust the automatic advance interval.

The standalone vendor build requires a host C compiler and Expat development
files to build its Wayland protocol generator.
