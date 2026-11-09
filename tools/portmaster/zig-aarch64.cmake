# Cross-compilation toolchain: Windows/Linux host -> aarch64 Linux (glibc), with Zig
# as the C/C++ compiler (static libc++, no libstdc++ needed on the device).
# Usage: set ZIG_EXE to zig(.exe) (pip install ziglang), then see build.py.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED ZIG_EXE AND DEFINED ENV{ZIG_EXE})
  set(ZIG_EXE "$ENV{ZIG_EXE}")
endif()
if(NOT ZIG_EXE)
  message(FATAL_ERROR "ZIG_EXE is not set (path to zig or zig.exe)")
endif()
file(TO_CMAKE_PATH "${ZIG_EXE}" ZIG_EXE)

# glibc 2.28 is old enough for muOS / Knulli / ArkOS / ROCKNIX firmwares.
set(ZIG_TARGET "aarch64-linux-gnu.2.28")
set(CMAKE_C_COMPILER "${ZIG_EXE}")
set(CMAKE_C_COMPILER_ARG1 "cc -target ${ZIG_TARGET}")
set(CMAKE_CXX_COMPILER "${ZIG_EXE}")
set(CMAKE_CXX_COMPILER_ARG1 "c++ -target ${ZIG_TARGET}")
set(CMAKE_AR "${ZIG_EXE}" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${ZIG_EXE}" CACHE FILEPATH "" FORCE)
foreach(lang C CXX)
  set(CMAKE_${lang}_ARCHIVE_CREATE "<CMAKE_AR> ar qc <TARGET> <OBJECTS>")
  set(CMAKE_${lang}_ARCHIVE_APPEND "<CMAKE_AR> ar q <TARGET> <OBJECTS>")
  set(CMAKE_${lang}_ARCHIVE_FINISH "<CMAKE_RANLIB> ranlib <TARGET>")
endforeach()
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
