
#pragma once

// std
#include <iostream>
#include <vector>

// glm
#include <glm/glm.hpp>

// project
#include <opengl.hpp>



namespace cgra {

	// A data structure for holding buffer IDs and other information related to drawing.
	// Also has a helper functions for drawing the mesh and deleting the gl buffers.
	// location 1 : positions (vec3)
	// location 2 : normals (vec3)
	// location 3 : uv (vec2)
	struct gl_mesh {
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ibo = 0;
		GLenum mode = 0; // mode to draw in, eg: GL_TRIANGLES
		int index_count = 0; // how many indicies to draw (no primitives)

		// calls the draw function on mesh data
		void draw();

		// deletes the gl buffers (cleans up all the data)
		void destroy();
	};


	struct mesh_vertex {
		glm::vec3 pos{0};
		glm::vec3 norm{0};
		glm::vec2 uv{0};
	};


	// Mesh builder object used to create an mesh by taking vertex and index information
	// and uploading them to OpenGL.
	struct mesh_builder {

		GLenum mode = GL_TRIANGLES;
		std::vector<mesh_vertex> vertices;
		std::vector<unsigned int> indices;

		mesh_builder() {}

		mesh_builder(GLenum mode_) : mode(mode_) {}

		template <size_t N, size_t M>
		explicit mesh_builder(const mesh_vertex(&vertData)[N], const mesh_vertex(&idxData)[M], GLenum mode_ = GL_TRIANGLES)
			: vertices(vertData, vertData+N), indices(idxData, idxData+M), mode(mode_) { }

		GLuint push_vertex(mesh_vertex v) {
			auto size = vertices.size();
			assert(size == decltype(size)(GLuint(size)));
			vertices.push_back(v);
			return GLuint(size);
		}

		void push_index(GLuint i) {
			indices.push_back(i);
		}

		void push_indices(std::initializer_list<GLuint> inds) {
			indices.insert(indices.end(), inds);
		}

		gl_mesh build() const;

		void print() const {
			std::cout << "pos" << std::endl;
			for (mesh_vertex v : vertices) {
				std::cout << v.pos.x << ", " << v.pos.y << ", " << v.pos.z << ", ";
				std::cout << v.norm.x << ", " << v.norm.y << ", " << v.norm.z << ", ";
				std::cout << v.uv.x << ", " << v.uv.y << ", " << std::endl;
			}
			std::cout << "idx" << std::endl;
			for (int i : indices) {
				std::cout << i << ", ";
			}
			std::cout << std::endl;
		}
	};

}

