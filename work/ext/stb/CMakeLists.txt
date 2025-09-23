add_library(stb
  stb.c
)

target_include_directories(stb PUBLIC "${PROJECT_SOURCE_DIR}/ext/stb")