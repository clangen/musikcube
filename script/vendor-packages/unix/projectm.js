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
    `projectm-${version}`, `projectm-${version}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/projectm/tar.gz/refs/tags/v${version}`,
  ],
  projectm_eval: [
    `projectm-eval-${evalRevision}`, `projectm-eval-${evalRevision}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/projectm-eval/tar.gz/${evalRevision}`,
  ],
  projectm_sdl_headers: [
    `SDL-release-${sdlVersion}`, `SDL2-${sdlVersion}.tar.gz`,
    `https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-${sdlVersion}`,
  ],
  projectm_imgui: [
    `imgui-${imguiVersion}`, `imgui-${imguiVersion}.tar.gz`,
    `https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v${imguiVersion}`,
  ],
  projectm_wayland_scanner: [
    'wayland-1.18.0', 'wayland-1.18.0.tar.gz',
    'https://codeload.github.com/wayland-mirror/wayland/tar.gz/refs/tags/1.18.0',
  ],
  projectm_presets: [
    `presets-en-d-${presetsRevision}`, `presets-en-d-${presetsRevision}.tar.gz`,
    `https://codeload.github.com/projectM-visualizer/presets-en-d/tar.gz/${presetsRevision}`,
  ],
};

const configureOptions = [
  '-DCMAKE_BUILD_TYPE=Release',
  '-DBUILD_SHARED_LIBS=OFF',
  '-DBUILD_TESTING=OFF',
  '-DENABLE_PLAYLIST=OFF',
  '-DENABLE_SYSTEM_PROJECTM_EVAL=OFF',
  '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
  '-DOPENGL_GL_PREFERENCE=LEGACY',
];

exports.build = function(context) {
  console.log('Building projectM');
  const evalDirectory = join(context.sources.projectm, 'vendor/projectm-eval');
  rmSync(evalDirectory, { recursive: true, force: true });
  cpSync(context.sources.projectm_eval, evalDirectory, { recursive: true });
  cpSync(context.sources.projectm_presets, join(context.out, 'share/projectM'), { recursive: true });
  const imgui = join(context.out, 'include/projectm-imgui');
  mkdirSync(join(imgui, 'backends'), { recursive: true });
  for (const file of ['imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp',
    'imgui.h', 'imgui_internal.h', 'imconfig.h', 'imstb_rectpack.h', 'imstb_textedit.h',
    'imstb_truetype.h', 'LICENSE.txt', 'backends/imgui_impl_opengl3.cpp',
    'backends/imgui_impl_opengl3.h', 'backends/imgui_impl_opengl3_loader.h',
    'backends/imgui_impl_sdl2.cpp', 'backends/imgui_impl_sdl2.h']) {
    cpSync(join(context.sources.projectm_imgui, file), join(imgui, file));
  }
  const sdlSource = context.sources.projectm_sdl_headers;
  const sdlBuild = join(sdlSource, 'build');
  mkdirSync(sdlBuild, { recursive: true });
  const scannerSource = join(context.sources.projectm_wayland_scanner, 'src');
  const scanner = join(sdlBuild, 'wayland-scanner');
  writeFileSync(join(sdlBuild, 'wayland-version.h'),
    '#define WAYLAND_VERSION_MAJOR 1\n#define WAYLAND_VERSION_MINOR 18\n' +
    '#define WAYLAND_VERSION_MICRO 0\n#define WAYLAND_VERSION "1.18.0"\n');
  context.run('cc', ['-std=gnu11', '-I' + sdlBuild, '-I' + scannerSource,
    join(scannerSource, 'scanner.c'), join(scannerSource, 'wayland-util.c'), '-lexpat', '-o', scanner],
  { env: process.env });
  context.run('cmake', [
    ...context.config.cmake, '-S', sdlSource, '-B', sdlBuild,
    '-DCMAKE_INSTALL_PREFIX=' + context.out, '-DWAYLAND_SCANNER=' + scanner,
    '-DCMAKE_BUILD_TYPE=Release', '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON',
    '-DSDL_STATIC_PIC=ON', '-DSDL_TEST=OFF', '-DSDL_AUDIO=OFF', '-DSDL_JOYSTICK=OFF',
    '-DSDL_HAPTIC=OFF', '-DSDL_SENSOR=OFF', '-DSDL_VULKAN=OFF', '-DSDL_WAYLAND_LIBDECOR=OFF',
  ], { env: context.config.env });
  const sdlConfig = readFileSync(join(sdlBuild, 'SDL_config.h.intermediate'), 'utf8');
  if (!sdlConfig.includes('#define SDL_VIDEO_DRIVER_X11 1') ||
      !sdlConfig.includes('#define SDL_VIDEO_DRIVER_WAYLAND 1')) {
    throw new Error('SDL must be built with both X11 and Wayland support');
  }
  rmSync(join(sdlBuild, 'wayland-generated-protocols'), { recursive: true, force: true });
  mkdirSync(join(sdlBuild, 'wayland-generated-protocols'), { recursive: true });
  context.run('cmake', ['--build', sdlBuild, '--parallel', context.config.jobs.slice(2)], { env: context.config.env });
  context.run('cmake', ['--install', sdlBuild], { env: context.config.env });
  buildCmake(context, 'projectm', configureOptions);
};
