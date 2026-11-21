#ifndef MUSIKCORE_MUSIKCORE_HPP
#define MUSIKCORE_MUSIKCORE_HPP

/* Header-only C++ interface to the public C API. Include this umbrella for
   request dispatch, library queries, metadata pages, and app-supplied playback;
   subsystem headers can be included separately when fewer facilities are needed. */
#include <musikcore/context.hpp>
#include <musikcore/library.hpp>
#include <musikcore/playback.hpp>
#include <musikcore/output.hpp>
#include <musikcore/equalizer.hpp>
#include <musikcore/indexer.hpp>
#include <musikcore/track-result-window.hpp>
#include <musikcore/visualizer.hpp>
#include <musikcore/preferences.hpp>
#include <musikcore/server.hpp>
#include <musikcore/plugins.hpp>

#include <musikcore/paths.hpp>

#endif
