# Test both valid and mixed-core consumers so the guard cannot pass by simply
# rejecting every build. The mixed case also checks a transitive dependency.
foreach(mixed IN ITEMS OFF ON)
  # These directories belong only to the configure fixture. A fresh cache keeps
  # moved sources and previous compiler/generator choices out of this contract.
  file(REMOVE_RECURSE "${BUILD}/${mixed}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${BUILD}/${mixed}"
    "-DGUARD=${GUARD}" "-DMIX_CORES=${mixed}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(mixed)
    # An unrelated configure failure must not count as detecting the mixed link.
    if(status EQUAL 0 OR NOT "${output}${error}" MATCHES "links both production musikcore and musikcore_abi0")
      message(FATAL_ERROR "Mixed core link was not rejected as expected: ${output}${error}")
    endif()
  elseif(NOT status EQUAL 0)
    message(FATAL_ERROR "Single core link was rejected: ${output}${error}")
  endif()
endforeach()
