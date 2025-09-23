
#pragma once

// std
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

// stb
#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>

// project
#include <opengl.hpp>


namespace cgra {

	// 4-channel rgba image
	struct rgba_image {
		glm::ivec2 size;
		std::vector<unsigned char> data;
		glm::vec<2, GLenum> wrap{GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};

		rgba_image() : size(0, 0) { }

		explicit rgba_image(int w, int h) : size(w, h), data(size.x * size.y * 4, 0) { }

		explicit rgba_image(glm::ivec2 size_) : size(size_), data(size.x * size.y * 4, 0) { }

		explicit rgba_image(const std::string &filename) {
			stbi_set_flip_vertically_on_load(true); // gl expects image origin at lower left
			unsigned char *raw_stb_data = stbi_load(filename.c_str(), &size.x, &size.y, nullptr, 4);
			if (!raw_stb_data) {
				std::cerr << "Error: Failed to open image " << filename << std::endl;
				throw std::runtime_error("Error: Failed to open image " + filename);
			}
			data.assign(raw_stb_data, raw_stb_data + size.x * size.y * 4);
			stbi_image_free(raw_stb_data);
		}

		// generates and returns a texture object
		GLuint uploadTexture(GLenum format = GL_RGBA8, GLuint tex = 0) const {
			assert(size.x * size.y * 4 == data.size()); // check we have consistent size and data

			if (!tex) glGenTextures(1, &tex);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap.x);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap.y);
			glTexImage2D(
				GL_TEXTURE_2D, 0, format, size.x, size.y, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, data.data()
			);
			glGenerateMipmap(GL_TEXTURE_2D);
			return tex;
		}


		// outputs the image to the given filepath and appends ".png"
		void writePng(const std::string &filename) {
			assert(size.x * size.y * 4 == data.size()); // check we have consistent size and data

			std::vector<unsigned char> char_data(size.x * size.y * 4, 0);
			std::ostringstream ss;
			ss << filename << ".png";
			if (stbi_write_png(ss.str().c_str(), size.x, size.y, 4, data.data() + (size.y - 1) * size.x * 4, -size.x * 4)) {
				std::cout << "Wrote image " << ss.str() << std::endl;
			} else {
				std::cerr << "Error: Failed to write image " << ss.str() << std::endl;
			}
		}


		// creates an image from FB0
		static rgba_image screenshot(bool write) {
			using namespace std;
			int w, h;
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);

			rgba_image img(w, h);
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.data.data());

			if (write) {
				ostringstream filename_ss;
				filename_ss << "screenshot_" << (chrono::system_clock::now().time_since_epoch() / 1ms);
				string filename = filename_ss.str();
				img.writePng(filename);
			}

			return img;
		}
	};

}
