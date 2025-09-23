
#pragma once

// std
#include <map>
#include <memory>
#include <string>

// project
#include <opengl.hpp>


namespace cgra {

	class shader_builder {
	private:
		std::map<GLenum, std::shared_ptr<gl_object>> m_shaders;

	public:
		shader_builder() { }
		void set_shader(GLenum type, const std::string &filename);
		void set_shader_source(GLenum type, const std::string &shadersource);

		GLuint build(GLuint program = 0);
	};

}