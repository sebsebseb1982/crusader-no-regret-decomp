# Fake SDL2 package for the PortMaster build: headers + an empty libSDL2-2.0.so.0
# to link against. At run time the device's own SDL2 is loaded (DT_NEEDED).
if(NOT TARGET SDL2::SDL2)
  add_library(SDL2::SDL2 SHARED IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES
    IMPORTED_LOCATION "${CRUSADER_SDL2_STUB_LIB}"
    IMPORTED_SONAME "libSDL2-2.0.so.0"
    INTERFACE_INCLUDE_DIRECTORIES "${CRUSADER_SDL2_HEADERS}")
endif()
set(SDL2_FOUND TRUE)
