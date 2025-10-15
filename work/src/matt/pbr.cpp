// std
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>

// glm
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "cgra/cgra_image.hpp"
#include "cgra/cgra_shader.hpp"
#include "matt/pbr.hpp"
#include "matt/render_utils.hpp"

textureData gold;
textureData plastic;

GLuint m_shader = 0;
GLuint m_default_shader = 0;
GLuint m_pbr_shader = 0;
GLuint m_cubemap_shader = 0;
GLuint m_irradiance_shader = 0;
GLuint m_prefilter_shader = 0;
GLuint m_brdf_shader = 0;
GLuint m_background_shader = 0;

int m_selected_shader = 0;

unsigned int irradianceMap = 0;
unsigned int prefilterMap = 0;
unsigned int brdfLUTTexture = 0;
unsigned int envCubemap = 0;
unsigned int hdrTexture = 0;

GLuint loadTexture(char const* path) {
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data) {
		GLenum format = GL_RGB; // default initialization
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else {
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}
	return textureID;
}

textureData loadPBRTextures(const std::string& basePath) {
	textureData tex;

	tex.albedo = loadTexture((basePath + "/albedo.png").c_str());
	tex.normal = loadTexture((basePath + "/normal.png").c_str());
	tex.metallic = loadTexture((basePath + "/metallic.png").c_str());
	tex.roughness = loadTexture((basePath + "/roughness.png").c_str());
	tex.ao = loadTexture((basePath + "/ao.png").c_str());

	return tex;
}

void bindPBRTextures(const textureData& tex) {
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, tex.albedo);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, tex.normal);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, tex.metallic);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, tex.roughness);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, tex.ao);
}

void loadPBRShaders() {
	glUseProgram(m_pbr_shader);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "irradianceMap"), 0);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "prefilterMap"), 1);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "brdfLUT"), 2);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "albedoMap"), 3);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "normalMap"), 4);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "metallicMap"), 5);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "roughnessMap"), 6);
	glUniform1i(glGetUniformLocation(m_pbr_shader, "aoMap"), 7);

	glUseProgram(m_background_shader);
	glUniform1i(glGetUniformLocation(m_background_shader, "environmentMap"), 0);

	// texture loading
	gold = loadPBRTextures(CGRA_SRCDIR + std::string("/res/textures/gold"));
	plastic = loadPBRTextures(CGRA_SRCDIR + std::string("/res/textures/plastic"));

	//pbr framebuffer
	unsigned int captureFBO;
	unsigned int captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1024, 1024);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	//load hdr environment map
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrComponents;
	float* data = stbi_loadf((CGRA_SRCDIR + std::string("//res//textures//space.hdr")).c_str(), &width, &height, &nrComponents, 0);
	if (data) {
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else {
		std::cout << "Failed to load HDR image." << std::endl;
	}

	// create cubemap
	glGenTextures(1, &envCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	for (unsigned int i = 0; i < 6; ++i) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
			1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// projection/view matrices for capturing data onto the 6 cubemap face directions
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
	};

	// convert HDR to cubemap
	glUseProgram(m_cubemap_shader);
	glUniform1i(glGetUniformLocation(m_cubemap_shader, "equirectangularMap"), 0);
	glUniformMatrix4fv(glGetUniformLocation(m_cubemap_shader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	glViewport(0, 0, 1024, 1024);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; i++) {
		glUniformMatrix4fv(glGetUniformLocation(m_cubemap_shader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// create irradiance cubemap
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	for (unsigned int i = 0; i < 6; ++i) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	// solve diffuse integral by convolution to create an irradiance cubemap
	glUseProgram(m_irradiance_shader);
	glUniform1i(glGetUniformLocation(m_irradiance_shader, "environmentMap"), 0);
	glUniformMatrix4fv(glGetUniformLocation(m_irradiance_shader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glViewport(0, 0, 32, 32);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; i++) {
		glUniformMatrix4fv(glGetUniformLocation(m_irradiance_shader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// pre-filter cubemap
	glGenTextures(1, &prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	for (unsigned int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 256, 256, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// run a quasi Monte Carlo simulation to generate a pre-filtered environment cubemap
	glUseProgram(m_prefilter_shader);
	glUniform1i(glGetUniformLocation(m_prefilter_shader, "environmentMap"), 0);
	glUniformMatrix4fv(glGetUniformLocation(m_prefilter_shader, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
		// reisze framebuffer according to mip-level size.
		unsigned int mipWidth = static_cast<unsigned int>(256 * std::pow(0.5, mip));
		unsigned int mipHeight = static_cast<unsigned int>(256 * std::pow(0.5, mip));
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		glUniform1f(glGetUniformLocation(m_prefilter_shader, "roughness"), roughness);
		for (unsigned int i = 0; i < 6; ++i) {
			glUniformMatrix4fv(glGetUniformLocation(m_prefilter_shader, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderCube();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// generate BRDF lookup texture
	glGenTextures(1, &brdfLUTTexture);

	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	// be sure to set wrapping mode to GL_CLAMP_TO_EDGE
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

	glViewport(0, 0, 512, 512);
	glUseProgram(m_brdf_shader);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// initialize static shader uniforms
	glm::mat4 projection = glm::perspective(glm::radians(90.0f), float(1280) / float(720), 0.1f, 100.f);
	glUseProgram(m_pbr_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "projection"), 1, false, value_ptr(projection));
	glUseProgram(m_background_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_background_shader, "projection"), 1, false, value_ptr(projection));
}

void buildShaders() {
	cgra::shader_builder sb;
	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_vert.glsl"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_frag.glsl"));
	m_default_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//pbr.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//pbr.fs"));
	m_pbr_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//cubemap.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//cubemap.fs"));
	m_cubemap_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//cubemap.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//irradiance.fs"));
	m_irradiance_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//cubemap.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//prefilter.fs"));
	m_prefilter_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//brdf.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//brdf.fs"));
	m_brdf_shader = sb.build();

	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//background.vs"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//background.fs"));
	m_background_shader = sb.build();

	loadPBRShaders();
}