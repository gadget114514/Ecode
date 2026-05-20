# copy_plugin.cmake
# Copies a plugin executable from its build output to output directories.
# Handles two output conventions:
#   1. ${src_dir}/bin/${config}/${name}.exe  (explicit RUNTIME_OUTPUT_DIRECTORY)
#   2. ${build_dir}/${config}/${name}.exe     (CMake default build-tree)
# Each plugin is built independently via its own CMakeLists.txt.

set(src_exe  "${src_dir}/bin/${config}/${name}.exe")
set(bld_exe  "${build_dir}/${config}/${name}.exe")
set(out_exe  "${out_dir}/${name}.exe")
set(plug_exe "${out_dir}/plugins/${name}.exe")

if(EXISTS "${src_exe}")
  set(exe "${src_exe}")
elseif(EXISTS "${bld_exe}")
  set(exe "${bld_exe}")
else()
  message(WARNING "${name}.exe not found:\n  ${src_exe}\n  ${bld_exe}")
  return()
endif()

# Copy plugin executable to plugins subdirectory only
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
