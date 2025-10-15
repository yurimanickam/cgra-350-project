#pragma once

// texture data struct
struct textureData {
	GLuint albedo = 0;
	GLuint normal = 0;
	GLuint metallic = 0;
	GLuint roughness = 0;
	GLuint ao = 0;
};

// textures
extern textureData gold;
extern textureData plastic;

// shaders
extern GLuint m_shader;
extern GLuint m_default_shader;
extern GLuint m_pbr_shader;
extern GLuint m_cubemap_shader;
extern GLuint m_irradiance_shader;
extern GLuint m_prefilter_shader;
extern GLuint m_brdf_shader;
extern GLuint m_background_shader;

extern int m_selected_shader;

extern unsigned int irradianceMap;
extern unsigned int prefilterMap;
extern unsigned int brdfLUTTexture;
extern unsigned int envCubemap;
extern unsigned int hdrTexture;

GLuint loadTexture(char const* path);
textureData loadPBRTextures(const std::string& basePath);
void bindPBRTextures(const textureData& tex);
void loadPBRShaders();
void buildShaders();