
// std
#include <stdexcept>

// project
#include "cgra_mesh.hpp"



using namespace glm;

namespace cgra {

	void gl_mesh::draw() {
		if (vao == 0) return;
		// bind our VAO which sets up all our buffers and data for us
		glBindVertexArray(vao);
		// tell opengl to draw our VAO using the draw mode and how many verticies to render
		glDrawElements(mode, index_count, GL_UNSIGNED_INT, 0);
	}

	void gl_mesh::destroy() {
		// delete the data buffers
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ibo);
	}


	gl_mesh mesh_builder::build() const {

		gl_mesh m;
		glGenVertexArrays(1, &m.vao); // VAO stores information about how the buffers are set up
		glGenBuffers(1, &m.vbo); // VBO stores the vertex data
		glGenBuffers(1, &m.ibo); // IBO stores the indices that make up primitives


		// VAO
		//
		glBindVertexArray(m.vao);

		
		// VBO (single buffer, interleaved)
		//
		glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
		// upload ALL the vertex data in one buffer
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(mesh_vertex), &vertices[0], GL_STATIC_DRAW);

		// this buffer will use location=0 when we use our VAO
		glEnableVertexAttribArray(0);
		// tell opengl how to treat data in location=0 - the data is treated in lots of 3 (3 floats = vec3)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_vertex), (void *)(offsetof(mesh_vertex, pos)));

		// do the same thing for Normals but bind it to location=1
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_vertex), (void *)(offsetof(mesh_vertex, norm)));

		// do the same thing for UVs but bind it to location=2 - the data is treated in lots of 2 (2 floats = vec2)
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(mesh_vertex), (void *)(offsetof(mesh_vertex, uv)));


		// IBO
		//
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
		// upload the indices for drawing primitives
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &indices[0], GL_STATIC_DRAW);


		// set the index count and draw modes
		m.index_count = indices.size();
		m.mode = mode;

		// clean up by binding VAO 0 (good practice)
		glBindVertexArray(0);

		return m;
	}
}