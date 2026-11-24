set (nullout_SOURCES
  supereq/Equ.cpp
  supereq/Fftsg_fl.c
  supereqdsp_plugin.cpp
  SuperEqDsp.cpp
)

add_library(supereqdsp SHARED ${nullout_SOURCES})
musikcube_configure_c_plugin(supereqdsp)
