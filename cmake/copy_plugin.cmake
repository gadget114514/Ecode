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
file(TO_NATIVE_PATH "${exe}"     _exe)
file(TO_NATIVE_PATH "${plug_exe}" _plug)
execute_process(
  COMMAND cmd.exe /c "copy /Y \"${_exe}\" \"${_plug}\" > nul & exit 0"
  OUTPUT_QUIET ERROR_QUIET
)
get_filename_component(_full "${exe}" ABSOLUTE)
message(STATUS "Copied ${_full} -> ${plug_exe}")
