# CMake Windows build dependencies module

include_guard(GLOBAL)

include(buildspec_common)

# _check_dependencies_windows: Set up Windows slice for _check_dependencies
function(_check_dependencies_windows)
  set(arch ${CMAKE_VS_PLATFORM_NAME})
  set(platform windows-${arch})

  set(dependencies_dir "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()

  set(onnxruntime_version "1.22.0")
  set(onnxruntime_archive "${dependencies_dir}/microsoft.ml.onnxruntime.directml.${onnxruntime_version}.nupkg")
  set(
    onnxruntime_url
    "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/${onnxruntime_version}/microsoft.ml.onnxruntime.directml.${onnxruntime_version}.nupkg"
  )
  set(
    onnxruntime_hash
    "fb9878ec0ccaa00dda700e68fd039a4fdd9f941a97f4346e3ecf7bfd25de4127180ec472c07ce634e8c3aff94ee42ff07339e5767eeefb5cba484011c6477031"
  )
  set(onnxruntime_root "${dependencies_dir}/onnxruntime-directml-${onnxruntime_version}")

  if(EXISTS "${onnxruntime_archive}")
    file(SHA512 "${onnxruntime_archive}" existing_onnxruntime_hash)
    if(NOT existing_onnxruntime_hash STREQUAL onnxruntime_hash)
      file(REMOVE "${onnxruntime_archive}")
    endif()
  endif()

  if(NOT EXISTS "${onnxruntime_archive}")
    file(
      DOWNLOAD "${onnxruntime_url}"
      "${onnxruntime_archive}"
      STATUS download_status
      EXPECTED_HASH "SHA512=${onnxruntime_hash}"
    )
    list(GET download_status 0 download_error)
    list(GET download_status 1 download_message)
    if(download_error GREATER 0)
      message(FATAL_ERROR "Unable to download ONNX Runtime: ${download_message}")
    endif()
  endif()

  if(
    NOT EXISTS "${onnxruntime_root}/runtimes/win-x64/native/onnxruntime.dll"
    OR NOT EXISTS "${onnxruntime_root}/LICENSE"
    OR NOT EXISTS "${onnxruntime_root}/ThirdPartyNotices.txt"
  )
    file(MAKE_DIRECTORY "${onnxruntime_root}")
    file(ARCHIVE_EXTRACT INPUT "${onnxruntime_archive}" DESTINATION "${onnxruntime_root}")
  endif()

  set(yunet_model "${dependencies_dir}/face_detection_yunet_2023mar.onnx")
  if(EXISTS "${yunet_model}")
    file(SHA256 "${yunet_model}" existing_yunet_hash)
    if(NOT existing_yunet_hash STREQUAL "8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4")
      file(REMOVE "${yunet_model}")
    endif()
  endif()
  if(NOT EXISTS "${yunet_model}")
    file(
      DOWNLOAD
        "https://github.com/opencv/opencv_zoo/raw/48b10fa505aeb51fbd127fc60a5c660040e823b7/models/face_detection_yunet/face_detection_yunet_2023mar.onnx"
      "${yunet_model}"
      STATUS download_status
      EXPECTED_HASH SHA256=8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4
    )
    list(GET download_status 0 download_error)
    list(GET download_status 1 download_message)
    if(download_error GREATER 0)
      message(FATAL_ERROR "Unable to download the YuNet face detection model: ${download_message}")
    endif()
  endif()

  set(ONNXRUNTIME_ROOT "${onnxruntime_root}" PARENT_SCOPE)
  set(YUNET_MODEL "${yunet_model}" PARENT_SCOPE)
endfunction()

_check_dependencies_windows()
