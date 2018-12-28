set (nullout_SOURCES
  supereq/Equ.cpp
  supereq/Fftsg_fl.c
  supereqdsp_plugin.cpp
  SuperEqDsp.cpp
)

add_library(supereqdsp SHARED ${nullout_SOURCES})
target_link_libraries(supereqdsp ${musikcube_LINK_LIBS})
