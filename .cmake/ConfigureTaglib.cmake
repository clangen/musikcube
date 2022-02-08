# we default to using a bundled version of taglib because the latest release
# is from 2016, and the upstream git repo is hundreds of commits ahead and
# has a number of important bugfixes.
if (NOT DEFINED ENABLE_BUNDLED_TAGLIB)
  message(STATUS "[build] ENABLE_BUNDLED_TAGLIB not defined, setting to 'true'")
  set(ENABLE_BUNDLED_TAGLIB "true")
else()
  message(STATUS "[build] ENABLE_BUNDLED_TAGLIB specified as '${ENABLE_BUNDLED_TAGLIB}'")
endif()
