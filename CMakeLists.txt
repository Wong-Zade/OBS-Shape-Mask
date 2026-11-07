cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)

if(OS_WINDOWS)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/face_tracker.c)
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${ONNXRUNTIME_ROOT}/build/native/include")

  install(
    FILES
      "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
      "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll"
    DESTINATION "obs-plugins/64bit"
  )

  install(FILES "${YUNET_MODEL}" DESTINATION "${CMAKE_PROJECT_NAME}/data")
  install(
    FILES "${ONNXRUNTIME_ROOT}/LICENSE" "${ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
    DESTINATION "${CMAKE_PROJECT_NAME}/data/licenses"
  )
  set(rundir "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}")
  set(rundir_plugin_bin "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/obs-plugins/64bit")
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${rundir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${YUNET_MODEL}" "${rundir}/yunet.onnx"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${rundir}/licenses"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${ONNXRUNTIME_ROOT}/LICENSE" "${rundir}/licenses/onnxruntime-LICENSE.txt"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
      "${rundir}/licenses/onnxruntime-ThirdPartyNotices.txt"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${rundir_plugin_bin}"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
      "${rundir_plugin_bin}/onnxruntime.dll"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime_providers_shared.dll"
      "${rundir_plugin_bin}/onnxruntime_providers_shared.dll"
    COMMENT "Copy face detection model to rundir"
    VERBATIM
  )
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
