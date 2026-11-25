const { cpSync, mkdirSync, readFileSync, rmSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');
const buildCmake = require('./build-with-cmake');

const version = '4.1.8';
const evalRevision = 'e8c311e86f4d8c082e96e02b6a33525034eee6d4';
const sdlVersion = '2.30.11';
const imguiVersion = '1.91.9b';
const presetsRevision = 'fff71ea81223109f3558351667eef851f2781c96';

exports.packages = {
  projectm: [
    `projectm-${version}`,
    `projectm-${version}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/projectm/tar.gz/refs/tags/v${version}`,
  ],
  projectm_eval: [
    `projectm-eval-${evalRevision}`,
    `projectm-eval-${evalRevision}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/projectm-eval/tar.gz/${evalRevision}`,
  ],
  projectm_sdl_headers: [
    `SDL-release-${sdlVersion}`,
    `SDL2-${sdlVersion}.tar.gz`,
    `https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-${sdlVersion}`,
  ],
  projectm_imgui: [
    `imgui-${imguiVersion}`,
    `imgui-${imguiVersion}.tar.gz`,
    `https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v${imguiVersion}`,
  ],
  projectm_wayland_scanner: [
    'wayland-1.18.0',
    'wayland-1.18.0.tar.gz',
    'https://codeload.github.com/wayland-mirror/wayland/tar.gz/refs/tags/1.18.0',
  ],
  projectm_presets: [
    `presets-en-d-${presetsRevision}`,
    `presets-en-d-${presetsRevision}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/presets-en-d/tar.gz/${presetsRevision}`,
  ],
};

/* macOS uses Cocoa and does not need the Linux Wayland protocol generator. */
if (process.platform === 'darwin') {
  delete exports.packages.projectm_wayland_scanner;
}

/* keep the engine static inside the optional plugin/helper. playback does not
need SDL audio, and the app supplies its own preset selection controls. */
const configureOptions = [
  '-DCMAKE_BUILD_TYPE=Release',
  '-DBUILD_SHARED_LIBS=OFF',
  '-DBUILD_TESTING=OFF',
  '-DENABLE_PLAYLIST=OFF',
  '-DENABLE_SYSTEM_PROJECTM_EVAL=OFF',
  '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
  '-DOPENGL_GL_PREFERENCE=LEGACY',
];

/* these patches come from https://github.com/johnneerdael/ProjectM-Android.
apply them to fresh vendor sources in this order: transition/shader state, float
parsing, translation cache, shapes. 0003 omits a change already present in
projectM 4.1.8. */
const patches = [
  'projectm-4.1.8-0002-render-target-resize-program-cache-shader-state.patch',
  'projectm-4.1.8-0003-hlslparser-classic-locale.patch',
  'projectm-4.1.8-0005-cache-translated-preset-shaders.patch',
  'projectm-4.1.8-custom-shapes.patch',
];

/* compile Dear ImGui with the plugin so its C++ ABI stays inside that binary. */
const imguiFiles = [
  'imgui.cpp',
  'imgui_draw.cpp',
  'imgui_tables.cpp',
  'imgui_widgets.cpp',
  'imgui.h',
  'imgui_internal.h',
  'imconfig.h',
  'imstb_rectpack.h',
  'imstb_textedit.h',
  'imstb_truetype.h',
  'LICENSE.txt',
  'backends/imgui_impl_opengl3.cpp',
  'backends/imgui_impl_opengl3.h',
  'backends/imgui_impl_opengl3_loader.h',
  'backends/imgui_impl_sdl2.cpp',
  'backends/imgui_impl_sdl2.h',
];

const sdlOptions = [
  '-DCMAKE_BUILD_TYPE=Release',
  '-DSDL_SHARED=OFF',
  '-DSDL_STATIC=ON',
  '-DSDL_STATIC_PIC=ON',
  '-DSDL_TEST=OFF',
  '-DSDL_AUDIO=OFF',
  '-DSDL_JOYSTICK=OFF',
  '-DSDL_HAPTIC=OFF',
  '-DSDL_SENSOR=OFF',
  '-DSDL_VULKAN=OFF',
  '-DSDL_WAYLAND_LIBDECOR=OFF',
];

exports.build = function(context) {
  console.log('Building projectM');

  const { sources, out, config, run } = context;
  const buildOptions = { env: config.env };

  /* fail here if a new projectM version no longer accepts a patch, rather than
  silently shipping a build with only some of the intended changes. */
  for (const patch of patches) {
    run('patch', ['--batch', '-p1', '-i', join(__dirname, 'patches', patch)], {
      cwd: sources.projectm,
      env: config.env,
    });
  }

  /* release archives omit this submodule; supply the pinned evaluator source. */
  const evalDirectory = join(sources.projectm, 'vendor/projectm-eval');
  rmSync(evalDirectory, { recursive: true, force: true });
  cpSync(sources.projectm_eval, evalDirectory, { recursive: true });

  cpSync(sources.projectm_presets, join(out, 'share/projectM'), { recursive: true });

  const imgui = join(out, 'include/projectm-imgui');
  mkdirSync(join(imgui, 'backends'), { recursive: true });

  for (const file of imguiFiles) {
    cpSync(join(sources.projectm_imgui, file), join(imgui, file));
  }

  /* keep fullscreen pointer coordinates in window points, matching the controls. */
  run('patch', [
    '--batch', '-p1', '-i',
    join(__dirname, 'patches/imgui-1.91.9b-fullscreen-mouse.patch'),
  ], { cwd: imgui, env: config.env });

  const sdlSource = sources.projectm_sdl_headers;
  const sdlBuild = join(sdlSource, 'build');
  mkdirSync(sdlBuild, { recursive: true });

  /* the protocol generator runs on the build machine, even when SDL targets
  another architecture. use the host compiler and host libraries for it. */
  const scannerOptions = [];
  if (process.platform !== 'darwin') {
    const scannerSource = join(sources.projectm_wayland_scanner, 'src');
    const scanner = join(sdlBuild, 'wayland-scanner');

    writeFileSync(join(sdlBuild, 'wayland-version.h'), [
      '#define WAYLAND_VERSION_MAJOR 1',
      '#define WAYLAND_VERSION_MINOR 18',
      '#define WAYLAND_VERSION_MICRO 0',
      '#define WAYLAND_VERSION "1.18.0"',
      '',
    ].join('\n'));

    run('cc', [
      '-std=gnu11',
      '-I' + sdlBuild,
      '-I' + scannerSource,
      join(scannerSource, 'scanner.c'),
      join(scannerSource, 'wayland-util.c'),
      '-lexpat',
      '-o', scanner,
    ], { env: process.env });

    scannerOptions.push('-DWAYLAND_SCANNER=' + scanner);
  }

  run('cmake', [
    ...config.cmake,
    '-S', sdlSource,
    '-B', sdlBuild,
    '-DCMAKE_INSTALL_PREFIX=' + out,
    ...scannerOptions,
    ...sdlOptions,
  ], buildOptions);

  /* SDL can silently disable a backend when headers are missing. require both
  Linux window systems so the packaged visualizer works on either desktop. */
  if (process.platform !== 'darwin') {
    const sdlConfig = readFileSync(join(sdlBuild, 'SDL_config.h.intermediate'), 'utf8');
    if (!sdlConfig.includes('#define SDL_VIDEO_DRIVER_X11 1') ||
        !sdlConfig.includes('#define SDL_VIDEO_DRIVER_WAYLAND 1')) {
      throw new Error('SDL must be built with both X11 and Wayland support');
    }

    /* discard generated protocols that may belong to an earlier configuration. */
    const protocols = join(sdlBuild, 'wayland-generated-protocols');
    rmSync(protocols, { recursive: true, force: true });
    mkdirSync(protocols, { recursive: true });
  }

  run('cmake', ['--build', sdlBuild, '--parallel', config.jobs.slice(2)], buildOptions);
  run('cmake', ['--install', sdlBuild], buildOptions);

  buildCmake(context, 'projectm', configureOptions);
};
