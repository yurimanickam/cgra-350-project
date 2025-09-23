#########################################################
# Helper methods
# Allows us to set up source folders in a nice way
#########################################################

# Set source groups to mirror real directory tree
function(target_source_group_tree target)
	get_target_property(sources ${target} SOURCES)
	foreach(source ${sources})
		get_filename_component(source ${source} ABSOLUTE)
		string(REPLACE ${PROJECT_SOURCE_DIR}/ "" rel ${source})
		if(rel)
			string(REGEX REPLACE "/([^/]*)$" "" rel ${rel})
			if(NOT rel STREQUAL source)
				string(REPLACE "/" "\\\\" rel ${rel})
				source_group(${rel} FILES ${source})
			endif()
		endif()
	endforeach()
endfunction()

# Add sources to a target relative to current CMakeLists.txt directory (CMAKE_CURRENT_SOURCE_DIR)
function(target_relative_sources target)
	foreach(source ${ARGN})
		target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
	endforeach()
endfunction()