# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-src")
  file(MAKE_DIRECTORY "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-build"
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix"
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/tmp"
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/src/discordgamesdkdownload-populate-stamp"
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/src"
  "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/src/discordgamesdkdownload-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/src/discordgamesdkdownload-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/herew/Documents/GitHub/musikcube/_deps/discordgamesdkdownload-subbuild/discordgamesdkdownload-populate-prefix/src/discordgamesdkdownload-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
