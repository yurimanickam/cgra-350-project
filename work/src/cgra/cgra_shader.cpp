
// std
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// project
#include "cgra_shader.hpp"
#include <opengl.hpp>


// forward declaration
class shader_error : public std::runtime_error {
public:
	explicit shader_error(const std::string &what_ = "Generic shader error.") : std::runtime_error(what_) { }
};


class shader_type_error : public shader_error {
public:
	explicit shader_type_error(const std::string &what_ = "Bad shader type.") : shader_error(what_) { }
};


class shader_compile_error : public shader_error {
public:
	explicit shader_compile_error(const std::string &what_ = "Shader compilation failed.") : shader_error(what_) { }
};


class shader_link_error : public shader_error {
public:
	explicit shader_link_error(const std::string &what_ = "Shader program linking failed.") : shader_error(what_) { }
};


void printShaderInfoLog(GLuint obj) {
	int infologLength = 0;
	int charsWritten = 0;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength > 1) {
		std::vector<char> infoLog(infologLength);
		glGetShaderInfoLog(obj, infologLength, &charsWritten, &infoLog[0]);
		std::cout << "CGRA Shader : " << "SHADER :\n" << &infoLog[0] << std::endl;
	}
}


void printProgramInfoLog(GLuint obj) {
	int infologLength = 0;
	int charsWritten = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	if (infologLength > 1) {
		std::vector<char> infoLog(infologLength);
		glGetProgramInfoLog(obj, infologLength, &charsWritten, &infoLog[0]);
		std::cout << "CGRA Shader : " << "PROGRAM :\n" << &infoLog[0] << std::endl;
	}
}


namespace cgra {

	void shader_builder::set_shader(GLenum type, const std::string &filename) {
		std::ifstream fileStream(filename);

		if (!fileStream) {
			std::cerr << "Error: Could not locate and open file " << filename << std::endl;
			throw std::runtime_error("Error: Could not locate and open file " + filename);
		}

		std::stringstream buffer;
		buffer << fileStream.rdbuf();

		try {
			set_shader_source(type, buffer.str());
		}
		catch (shader_compile_error &e) {
			std::cerr << "Error: Could not compile " << filename << std::endl;
			throw e;
		}
	}


	void shader_builder::set_shader_source(GLenum type, const std::string &source) {

		// same as GLint shader = glCreateShader(type);
		gl_object shader = gl_object::gen_shader(type);

		// cgra specific extra (allows different shaders to be defined in a single source)
		// Start of CGRA addition
		//
		const auto get_define = [](GLenum stype) {
			switch (stype) {
			case GL_VERTEX_SHADER:
				return "_VERTEX_";
			case GL_GEOMETRY_SHADER:
				return "_GEOMETRY_";
			case GL_TESS_CONTROL_SHADER:
				return "_TESS_CONTROL_";
			case GL_TESS_EVALUATION_SHADER:
				return "_TESS_EVALUATION_";
			case GL_FRAGMENT_SHADER:
				return "_FRAGMENT_";
			default:
				return "_INVALID_SHADER_TYPE_";
			}
		};

		std::istringstream iss(source);
		std::ostringstream oss;
		while (iss) {
			std::string line;
			std::getline(iss, line);
			oss << line << std::endl;
			if (line.find("#version") < line.find("//"))
				break;
		}
		oss << "#define " << get_define(type) << std::endl;
		oss << iss.rdbuf();
		std::string final_source = oss.str();
		//
		// End of CGRA addition

		// upload and compile the shader
		const char *text_c = final_source.c_str();
		glShaderSource(shader, 1, &text_c, nullptr);
		glCompileShader(shader);

		// check compilation status
		GLint compile_status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
		printShaderInfoLog(shader); // print warnings and errors
		if (!compile_status) throw shader_compile_error();

		m_shaders[type] = std::make_shared<gl_object>(std::move(shader));
	}


	GLuint shader_builder::build(GLuint program) {

		// if the program exists get attached shaders and detach them
		if (program) {
			int shader_count = 0;
			glGetProgramiv(program, GL_ATTACHED_SHADERS, &shader_count);

			if (shader_count > 0) {
				std::vector<GLuint> attached_shaders(shader_count);
				int actual_shader_count = 0;
				glGetAttachedShaders(program, shader_count, &actual_shader_count, attached_shaders.data());
				for (int i = 0; i < actual_shader_count; i++) {
					glDetachShader(program, attached_shaders[i]);
				}
			}
		}
		else {
			program = glCreateProgram();
		}

		// attach shaders
		for (auto &shader_pair : m_shaders) {
			glAttachShader(program, *(shader_pair.second));
		}

		// link the program
		glLinkProgram(program);

		// check link status
		GLint link_status;
		glGetProgramiv(program, GL_LINK_STATUS, &link_status);
		printProgramInfoLog(program); // print warnings and errors
		if (!link_status) throw shader_link_error();

		return program;
	}

}