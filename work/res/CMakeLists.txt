# add resources as targets so they show up in IDEs

# list your shader source files here
SET(SHADERS
	
)

add_custom_target(
	res
	SOURCES
	${SHADERS}
)

source_group("shader" FILES ${SHADERS})
