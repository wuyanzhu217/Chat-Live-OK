# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\appccvWriter_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\appccvWriter_autogen.dir\\ParseCache.txt"
  "appccvWriter_autogen"
  )
endif()
