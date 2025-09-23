//----------------------------------------------------------------------------
//
// OpenGL header
// Set up to include eveything nessesary for OpenGL functionality in one place
//
//----------------------------------------------------------------------------

#pragma once

// include glew.h before (instead of) gl.h, or anything that includes gl.h
// glew.h replaces gl.h and sets up OpenGL functions in a cross-platform manner
#include <GL/glew.h>
#include <GLFW/glfw3.h>



namespace cgra {

	// helper function that draws an empty OpenGL object
	// can be used for shaders that do all the work
	inline void draw_dummy(unsigned instances = 1) {
		static GLuint vao = 0;
		if (vao == 0) {
			glGenVertexArrays(1, &vao);
		}
		glBindVertexArray(vao);
		glDrawArraysInstanced(GL_POINTS, 0, 1, instances);
		glBindVertexArray(0);
	}


	// gl_object is a helper class that wraps around a GLuint
	// object id for OpenGL. Does not allow copying (can't be
	// owned by more than one thing) and deallocates the object
	// when destroyed (easy cleanup). Can be used for VAO, VBO,
	// textures and frambuffers etc.
	class gl_object {
	public:
		using destroyer_t = void (APIENTRY *)(GLsizei, const GLuint *);

	private:
		GLuint m_id = 0;
		destroyer_t m_dtor;

		void destroy() noexcept {
			if (m_id) {
				m_dtor(1, &m_id);
				m_id = 0;
			}
		}

	public:

		// empty object
		gl_object() { }

		// explicit ctor
		// takes an existing OpenGL object identifier and a pointer to a function
		// that will deallocated the identifier on destruction
		gl_object(GLuint id_, destroyer_t dtor_) : m_id(id_), m_dtor(dtor_) { }

		// remove copy ctors
		gl_object(const gl_object &) = delete;
		gl_object & operator=(const gl_object &) = delete;


		// define move ctors
		gl_object(gl_object &&other) noexcept {
			m_id = other.m_id;
			m_dtor = other.m_dtor;
			other.m_id = 0;
		}

		gl_object & operator=(gl_object &&other) noexcept {
			destroy();
			m_id = other.m_id;
			m_dtor = other.m_dtor;
			other.m_id = 0;
			return *this;
		}

		// implicit GLuint converter
		// returns the OpenGL identifier for this object
		operator GLuint() const noexcept {
			return m_id;
		}

		// explicit boolean converter
		// true IFF identifier is not zero
		explicit operator bool() const noexcept {
			return m_id;
		}

		// true IFF identifier is zero
		bool operator!() const noexcept {
			return !m_id;
		}

		// destructor 
		~gl_object() {
			destroy();
		}

		// returns a gl_object with an OpenGL buffer identifier
		static gl_object gen_buffer() {
			GLuint o;
			glGenBuffers(1, &o);
			return { o, glDeleteBuffers };
		}

		// returns a gl_object with an OpenGL vertex array identifier
		static gl_object gen_vertex_array() {
			GLuint o;
			glGenVertexArrays(1, &o);
			return { o, glDeleteVertexArrays };
		}

		// returns a gl_object with an OpenGL texture identifier
		static gl_object gen_texture() {
			GLuint o;
			glGenTextures(1, &o);
			return { o, glDeleteTextures };
		}

		// returns a gl_object with an OpenGL framebuffer identifier
		static gl_object gen_framebuffer() {
			GLuint o;
			glGenFramebuffers(1, &o);
			return { o, glDeleteFramebuffers };
		}

		// returns a gl_object with an OpenGL shader identifier
		static gl_object gen_shader(GLenum type) {
			GLuint o = glCreateShader(type);
			return { o, [](GLsizei, const GLuint *o) { glDeleteShader(*o); } };
		}

		// returns a gl_object with an OpenGL shader program identifier
		static gl_object gen_program() {
			GLuint o = glCreateProgram();
			return { o, [](GLsizei, const GLuint *o) { glDeleteProgram(*o); } };
		}
	};
}