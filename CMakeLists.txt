cmake_minimum_required(VERSION 3.20)

project(AppForPowerSupply10A_Uppsala LANGUAGES CXX)

# The project is Linux-only
if(NOT UNIX OR APPLE)
  message(FATAL_ERROR "This project is intended to be built on Linux (e.g., Linux Mint) only.")
endif()

add_executable(AppForPowerSupply10A_Uppsala
  source/main.cpp
  source/gui_widgets.cpp
#  source/peripheral_thread.cpp
#  source/shared_data.cpp
#  source/modified_rstl_master.cpp
#  source/settings_file.cpp
)


target_compile_options(AppForPowerSupply10A_Uppsala PRIVATE
  -g
  -Wall
  -Wextra
  -Wpedantic
)


target_link_options(AppForPowerSupply10A_Uppsala PRIVATE
  -g
  -rdynamic
)


target_include_directories(AppForPowerSupply10A_Uppsala PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)




target_link_libraries(AppForPowerSupply10A_Uppsala PRIVATE
  fltk
  X11
  pthread
  fltk_images
  png
  z
)



# ------------------------------------------------------------------------------
# Copying files to the build directory
# ------------------------------------------------------------------------------
set_target_properties(AppForPowerSupply10A_Uppsala PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_custom_command(TARGET AppForPowerSupply10A_Uppsala POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/KwadrupoleWLiniiPionowej.cfg"
          "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>/KwadrupoleWLiniiPionowej.cfg"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/doc/KwadrupoleWLiniiPionowej.pdf"
          "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>/KwadrupoleWLiniiPionowej.pdf"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/doc/AppForPowerSupply10A_Uppsala.png"
          "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>/AppForPowerSupply10A_Uppsala.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/doc/AppForPowerSupply10A_Uppsala.svg"
          "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>/AppForPowerSupply10A_Uppsala.svg"
  COMMENT "Copying runtime resources to build output directory"
)

# ------------------------------------------------------------------------------
# Sanitizery: equivalents of targets make asan/ubsan/sanitize + run-*
# ------------------------------------------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN OR ENABLE_UBSAN)
  target_compile_options(AppForPowerSupply10A_Uppsala PRIVATE -O1 -fno-omit-frame-pointer)
  target_link_options(AppForPowerSupply10A_Uppsala PRIVATE -O1 -fno-omit-frame-pointer)
endif()

if(ENABLE_ASAN)
  target_compile_options(AppForPowerSupply10A_Uppsala PRIVATE -fsanitize=address)
  target_link_options(AppForPowerSupply10A_Uppsala PRIVATE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
  target_compile_options(AppForPowerSupply10A_Uppsala PRIVATE -fsanitize=undefined)
  target_link_options(AppForPowerSupply10A_Uppsala PRIVATE -fsanitize=undefined)
endif()

add_custom_target(run
  COMMAND "$<TARGET_FILE:AppForPowerSupply10A_Uppsala>"
  DEPENDS AppForPowerSupply10A_Uppsala
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>"
)

add_custom_target(run-asan
  COMMAND ${CMAKE_COMMAND} -E env "ASAN_OPTIONS=detect_leaks=1:halt_on_error=0"
          "$<TARGET_FILE:AppForPowerSupply10A_Uppsala>"
  DEPENDS AppForPowerSupply10A_Uppsala
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>"
)

add_custom_target(run-ubsan
  COMMAND ${CMAKE_COMMAND} -E env "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
          "$<TARGET_FILE:AppForPowerSupply10A_Uppsala>"
  DEPENDS AppForPowerSupply10A_Uppsala
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>"
)

add_custom_target(run-sanitize
  COMMAND ${CMAKE_COMMAND} -E env
          "ASAN_OPTIONS=detect_leaks=1:halt_on_error=0"
          "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
          "$<TARGET_FILE:AppForPowerSupply10A_Uppsala>"
  DEPENDS AppForPowerSupply10A_Uppsala
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:AppForPowerSupply10A_Uppsala>"
)

