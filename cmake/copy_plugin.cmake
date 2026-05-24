# copy_plugin.cmake
# Copies a plugin executable (and any sibling DLLs) to the output plugins/ directory.
# Checks these paths in order:
#   1. ${src_dir}/bin/${config}/plugins/${name}.exe  (plugin placed its own plugins/ subdir)
#   2. ${src_dir}/bin/${config}/${name}.exe           (explicit RUNTIME_OUTPUT_DIRECTORY)
#   3. ${build_dir}/${config}/${name}.exe             (CMake default build-tree)

set(plug_src_exe "${src_dir}/bin/${config}/plugins/${name}.exe")
set(src_exe      "${src_dir}/bin/${config}/${name}.exe")
set(bld_exe      "${build_dir}/${config}/${name}.exe")
set(plug_exe     "${out_dir}/plugins/${name}.exe")

if(EXISTS "${plug_src_exe}")
  set(exe "${plug_src_exe}")
  set(dll_dir "${src_dir}/bin/${config}/plugins")
elseif(EXISTS "${src_exe}")
  set(exe "${src_exe}")
  set(dll_dir "")
elseif(EXISTS "${bld_exe}")
  set(exe "${bld_exe}")
  set(dll_dir "")
else()
  message(WARNING "${name}.exe not found:\n  ${plug_src_exe}\n  ${src_exe}\n  ${bld_exe}")
  return()
endif()

# Copy plugin executable to plugins subdirectory
get_filename_component(_full "${exe}" ABSOLUTE)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${exe}" "${plug_exe}"
  RESULT_VARIABLE _copy_ret
)
if(_copy_ret EQUAL 0)
  message(STATUS "Copied ${_full} -> ${plug_exe}")
else()
  message(WARNING "Failed to copy ${_full} -> ${plug_exe} (error ${_copy_ret})")
endif()

# Copy sibling DLLs and helper EXEs when the plugin ships its own runtime files
if(dll_dir)
  file(GLOB _dlls "${dll_dir}/*.dll")
  foreach(_dll ${_dlls})
    get_filename_component(_dll_name "${_dll}" NAME)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${out_dir}/plugins/${_dll_name}"
      RESULT_VARIABLE _dll_ret
    )
    if(_dll_ret EQUAL 0)
      message(STATUS "Copied ${_dll} -> ${out_dir}/plugins/${_dll_name}")
    else()
      message(WARNING "Failed to copy ${_dll} -> ${out_dir}/plugins/${_dll_name}")
    endif()
  endforeach()

  # Copy sibling EXEs other than the plugin itself (e.g. OpenConsole.exe)
  file(GLOB _exes "${dll_dir}/*.exe")
  foreach(_exe ${_exes})
    get_filename_component(_exe_name "${_exe}" NAME)
    if(NOT _exe_name STREQUAL "${name}.exe")
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_exe}" "${out_dir}/plugins/${_exe_name}"
        RESULT_VARIABLE _exe_ret
      )
      if(_exe_ret EQUAL 0)
        message(STATUS "Copied ${_exe} -> ${out_dir}/plugins/${_exe_name}")
      else()
        message(WARNING "Failed to copy ${_exe} -> ${out_dir}/plugins/${_exe_name}")
      endif()
    endif()
  endforeach()
endif()
