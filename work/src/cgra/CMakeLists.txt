
# Source files
set(sources	
	"cgra_geometry.hpp"
	"cgra_geometry.cpp"

	"cgra_gui.hpp"
	"cgra_gui.cpp"
	
	"cgra_image.hpp"

	"cgra_mesh.hpp"
	"cgra_mesh.cpp"

	"cgra_shader.hpp"
	"cgra_shader.cpp"

	"cgra_wavefront.hpp"

	"CMakeLists.txt"
)

# Add these sources to the project target
target_relative_sources(${CGRA_PROJECT} ${sources})
