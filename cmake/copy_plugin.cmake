get_filename_component(dst_dir "${dst}" DIRECTORY)
get_filename_component(dst_name "${dst}" NAME)
file(MAKE_DIRECTORY "${dst_dir}")
file(MAKE_DIRECTORY "${plugindir}")

if(EXISTS "${src1}")
  configure_file("${src1}" "${dst}" COPYONLY)
elseif(EXISTS "${src2}")
  configure_file("${src2}" "${dst}" COPYONLY)
endif()

if(EXISTS "${dst}")
  configure_file("${dst}" "${plugindir}/${dst_name}" COPYONLY)
endif()
