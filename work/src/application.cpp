// std
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>

// glm
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// project
#include "application.hpp"
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_gui.hpp"
#include "cgra/cgra_image.hpp"
#include "cgra/cgra_shader.hpp"
#include "cgra/cgra_wavefront.hpp"

using namespace std;
using namespace cgra;
using namespace glm;

void basic_model::draw(const glm::mat4& view, const glm::mat4 proj) {
	mat4 modelview = view * modelTransform;

	glUseProgram(shader); // load shader and variables
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelview));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	mesh.draw(); // draw
}

Application::Application(GLFWwindow* window) : m_window(window) {
	shader_builder sb;
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

	m_shader = m_default_shader;
	m_model.shader = m_shader;
	//m_model.mesh = load_wavefront_data(CGRA_SRCDIR + std::string("/res//assets//teapot.obj")).build();
	m_model.color = vec3(1, 0, 0);

	// Initialize lava lamp
	initializeLavaLamp();
}

void Application::ensureDepthFBO(int width, int height) {
	// If existing size matches, nothing to do
	if (m_depthFBO != 0 && m_depthTexW == width && m_depthTexH == height)
		return;

	// Delete old if present
	if (m_depthFBO != 0) {
		glDeleteFramebuffers(1, &m_depthFBO);
		m_depthFBO = 0;
	}
	if (m_depthTextureFront != 0) {
		glDeleteTextures(1, &m_depthTextureFront);
		m_depthTextureFront = 0;
	}
	if (m_depthTextureBack != 0) {
		glDeleteTextures(1, &m_depthTextureBack);
		m_depthTextureBack = 0;
	}

	// Create two depth textures (front and back)
	glGenTextures(1, &m_depthTextureFront);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureFront);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// Ensure sampler sampling returns float not depth-compare on some drivers
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	// define base/max level to avoid "no base level" warnings
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glGenTextures(1, &m_depthTextureBack);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureBack);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Create FBO (we will attach either texture as depth attachment for each pass)
	glGenFramebuffers(1, &m_depthFBO);
	m_depthTexW = width;
	m_depthTexH = height;
}

void Application::initializeLavaLamp() {
	// Build lava lamp shader
	shader_builder lava_sb;
	lava_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lava_vertex.glsl"));
	lava_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//lava_fragment.glsl"));
	m_lavaShader = lava_sb.build();

	// Initialize the lava lamp simulation with 5 blobs
	m_lavaLamp.initialize(5);

	m_threshold = 1.0f;         // Lower threshold
	m_heaterTemp = 120.0f;      // increased heater for stronger rise
	m_gravity = -9.8f;          // fixed gravity (permanent)

	// Apply these settings to the lamp (set once)
	m_lavaLamp.setThreshold(m_threshold);
	m_lavaLamp.setHeaterTemperature(m_heaterTemp);
	m_lavaLamp.setGravity(m_gravity);
	// viscosity is automatic now; no setViscosity call

	// Create lamp container mesh
	m_lampGlassMesh = createLampContainerGlass();
	m_lampMetalMesh = createLampContainerMetal();
	m_fullscreenQuad = createFullscreenQuad();

	// Depth FBO will be created lazily in render (size depends on framebuffer)
	m_depthFBO = 0;
	m_depthTextureFront = 0;
	m_depthTextureBack = 0;
	m_depthTexW = m_depthTexH = 0;

	// Set initial time
	m_lastTime = static_cast<float>(glfwGetTime());
}

cgra::gl_mesh Application::createFullscreenQuad() {
	mesh_builder builder;

	// Create a large quad that covers the screen
	mesh_vertex v0, v1, v2, v3;
	v0.pos = vec3(-1, -1, 0); v0.uv = vec2(0, 0);
	v1.pos = vec3(1, -1, 0);  v1.uv = vec2(1, 0);
	v2.pos = vec3(1, 1, 0);   v2.uv = vec2(1, 1);
	v3.pos = vec3(-1, 1, 0);  v3.uv = vec2(0, 1);

	builder.push_vertex(v0);
	builder.push_vertex(v1);
	builder.push_vertex(v2);
	builder.push_vertex(v3);

	builder.push_index(0); builder.push_index(1); builder.push_index(2);
	builder.push_index(0); builder.push_index(2); builder.push_index(3);

	return builder.build();
}

cgra::gl_mesh Application::createLampContainerGlass() {
	mesh_builder builder;
	const int segments = 64;

	struct ProfilePoint {
		float height;
		float radius;
	};

	// Only bottom and top rings (smooth glass shell)
	std::vector<ProfilePoint> profile = {
		{1.7f, 1.8f},  // bottom of glass (just above the metal section)
		{10.0f, 1.0f}  // top ring
	};

	std::vector<int> ringStarts;
	for (size_t p = 0; p < profile.size(); ++p) {
		ringStarts.push_back(builder.vertices.size());
		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x = profile[p].radius * cos(angle);
			float z = profile[p].radius * sin(angle);
			float y = profile[p].height;

			mesh_vertex v;
			v.pos = vec3(x, y, z);
			v.norm = normalize(vec3(x, 0.0f, z));
			v.uv = vec2(float(i) / segments, float(p) / (profile.size() - 1));
			builder.push_vertex(v);
		}
	}

	// Connect the two rings into quads
	int currentRing = ringStarts[0];
	int nextRing = ringStarts[1];
	for (int i = 0; i < segments; ++i) {
		int curr = currentRing + i;
		int next = currentRing + i + 1;
		int currAbove = nextRing + i;
		int nextAbove = nextRing + i + 1;

		builder.push_index(curr);
		builder.push_index(currAbove);
		builder.push_index(next);

		builder.push_index(next);
		builder.push_index(currAbove);
		builder.push_index(nextAbove);
	}

	return builder.build();
}

cgra::gl_mesh Application::createLampContainerMetal() {
	mesh_builder builder;
	const int segments = 64;

	// ------------------------------
	// Lower third of bulb (metal)
	// ------------------------------
	{
		struct ProfilePoint {
			float height;
			float radius;
		};

		std::vector<ProfilePoint> lowerBulb = {
			{0.0f, 1.2f},  // bottom ring
			{1.7f, 1.8f}   // cut height
		};

		std::vector<int> ringStarts;
		for (size_t p = 0; p < lowerBulb.size(); ++p) {
			ringStarts.push_back(builder.vertices.size());
			for (int i = 0; i <= segments; ++i) {
				float angle = 2.0f * pi<float>() * i / segments;
				float x = lowerBulb[p].radius * cos(angle);
				float z = lowerBulb[p].radius * sin(angle);
				float y = lowerBulb[p].height;

				mesh_vertex v;
				v.pos = vec3(x, y, z);
				v.norm = normalize(vec3(x, 0.0f, z));
				v.uv = vec2(float(i) / segments, float(p) / (lowerBulb.size() - 1));
				builder.push_vertex(v);
			}
		}

		for (size_t p = 0; p < lowerBulb.size() - 1; ++p) {
			int currentRing = ringStarts[p];
			int nextRing = ringStarts[p + 1];
			for (int i = 0; i < segments; ++i) {
				int curr = currentRing + i;
				int next = currentRing + i + 1;
				int currAbove = nextRing + i;
				int nextAbove = nextRing + i + 1;

				builder.push_index(curr);
				builder.push_index(currAbove);
				builder.push_index(next);

				builder.push_index(next);
				builder.push_index(currAbove);
				builder.push_index(nextAbove);
			}
		}
	}

	// ------------------------------
	// Glass bottom cap (inverted)
	// ------------------------------
	{
		float capHeight = 0.0f;
		float capDepth = 0.8f;
		float capRadius = 1.2f;
		int capStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float xOuter = capRadius * cos(angle);
			float zOuter = capRadius * sin(angle);

			mesh_vertex vTop;
			vTop.pos = vec3(xOuter, capHeight, zOuter);
			vTop.norm = normalize(vec3(xOuter, 0, zOuter));
			vTop.uv = vec2(float(i) / segments, 0.0f);

			mesh_vertex vBottom;
			vBottom.pos = vec3(0, capHeight - capDepth, 0);
			vBottom.norm = vec3(0, -1, 0);
			vBottom.uv = vec2(0.5f, 1.0f);

			builder.push_vertex(vTop);
			builder.push_vertex(vBottom);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = capStart + i * 2;
			int i1 = capStart + i * 2 + 1;
			int i2 = capStart + (i + 1) * 2;
			int i3 = capStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}
	}

	// ------------------------------
	// Glass top cap
	// ------------------------------
	{
		float capBottomY = 10.0f;
		float capTopY = 11.0f;
		float capRadiusBottom = 1.0f;
		float capRadiusTop = 0.8f;
		int capStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x1 = capRadiusBottom * cos(angle);
			float z1 = capRadiusBottom * sin(angle);
			float x2 = capRadiusTop * cos(angle);
			float z2 = capRadiusTop * sin(angle);

			mesh_vertex v1, v2;
			v1.pos = vec3(x1, capBottomY, z1);
			v2.pos = vec3(x2, capTopY, z2);
			v1.norm = normalize(vec3(x1, 0, z1));
			v2.norm = normalize(vec3(x2, 0, z2));

			builder.push_vertex(v1);
			builder.push_vertex(v2);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = capStart + i * 2;
			int i1 = capStart + i * 2 + 1;
			int i2 = capStart + (i + 1) * 2;
			int i3 = capStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}

		int topCenter = builder.vertices.size();
		mesh_vertex vTopCenter;
		vTopCenter.pos = vec3(0, capTopY, 0);
		vTopCenter.norm = vec3(0, 1, 0);
		vTopCenter.uv = vec2(0.5f, 0.5f);
		builder.push_vertex(vTopCenter);

		for (int i = 0; i < segments; ++i) {
			int outer0 = capStart + i * 2 + 1;
			int outer1 = capStart + (i + 1) * 2 + 1;
			builder.push_index(topCenter);
			builder.push_index(outer0);
			builder.push_index(outer1);
		}
	}

	// ------------------------------
	// Metal base
	// ------------------------------
	{
		float baseHeight = -1.5f;
		float baseRadiusBottom = 2.5f;
		float baseRadiusTop = 1.2f;
		int baseStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x1 = baseRadiusBottom * cos(angle);
			float z1 = baseRadiusBottom * sin(angle);
			float y1 = baseHeight;

			float x2 = baseRadiusTop * cos(angle);
			float z2 = baseRadiusTop * sin(angle);
			float y2 = 0.0f;

			mesh_vertex v1, v2;
			v1.pos = vec3(x1, y1, z1);
			v2.pos = vec3(x2, y2, z2);
			v1.norm = normalize(vec3(x1, 0, z1));
			v2.norm = normalize(vec3(x2, 0, z2));
			v1.uv = vec2(float(i) / segments, 0.0f);
			v2.uv = vec2(float(i) / segments, 1.0f);

			builder.push_vertex(v1);
			builder.push_vertex(v2);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = baseStart + i * 2;
			int i1 = baseStart + i * 2 + 1;
			int i2 = baseStart + (i + 1) * 2;
			int i3 = baseStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}

		int baseBottomCenter = builder.vertices.size();
		mesh_vertex baseBottomV;
		baseBottomV.pos = vec3(0, baseHeight, 0);
		baseBottomV.norm = vec3(0, -1, 0);
		baseBottomV.uv = vec2(0.5f, 0.5f);
		builder.push_vertex(baseBottomV);

		for (int i = 0; i < segments; ++i) {
			int outer0 = baseStart + i * 2;
			int outer1 = baseStart + (i + 1) * 2;
			builder.push_index(baseBottomCenter);
			builder.push_index(outer1);
			builder.push_index(outer0);
		}
	}

	return builder.build();
}

/// IMPORTANT to be fixed, render layers completely broken so fix
void Application::renderLavaLamp(const glm::mat4& view, const glm::mat4& proj) {


	if (!m_showLavaLamp) return;


	// Save current state
	GLboolean depthMask;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	GLint depthFunc;
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
	GLboolean blendEnabled = glIsEnabled(GL_BLEND);


	// retrieve the window size
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	// Ensure depth FBO/textures exist at current size
	ensureDepthFBO(width, height);

	// Update simulation
	if (m_animateLamp) {
		float currentTime = static_cast<float>(glfwGetTime());
		float deltaTime = currentTime - m_lastTime;
		m_lastTime = currentTime;
		deltaTime = std::min(deltaTime, 0.05f);
		m_lavaLamp.update(deltaTime);
	}

	glUseProgram(m_lavaShader);

	// Set up matrices
	mat4 model = mat4(1.0f);
	mat4 modelView = view * model;
	mat4 normalMatrix = transpose(inverse(model));

	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uModelMatrix"), 1, GL_FALSE, value_ptr(model));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uNormalMatrix"), 1, GL_FALSE, value_ptr(normalMatrix));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uViewMatrix"), 1, GL_FALSE, value_ptr(view));

	// Pass inverse matrices for raymarching
	mat4 invProj = inverse(proj);
	mat4 invView = inverse(view);
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uInvProjectionMatrix"), 1, GL_FALSE, value_ptr(invProj));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uInvViewMatrix"), 1, GL_FALSE, value_ptr(invView));

	// Set time uniform
	glUniform1f(glGetUniformLocation(m_lavaShader, "uTime"), static_cast<float>(glfwGetTime()));

	// Set camera position (world-space)
	vec3 cameraPos = vec3(inverse(view) * vec4(0, 0, 0, 1));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uCameraPos"), 1, value_ptr(cameraPos));

	// Pass framebuffer resolution
	m_windowsize = vec2(width, height);
	glUniform2fv(glGetUniformLocation(m_lavaShader, "uResolution"), 1, value_ptr(m_windowsize));

	// Lamp parameters (use simulation getters so geometry + sim match)
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampRadius"), m_lavaLamp.getRadius());
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampTopRadius"), 1.0f); // adjust/getter if needed
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampHeight"), m_lavaLamp.getHeight());
	glUniform1f(glGetUniformLocation(m_lavaShader, "uThreshold"), m_threshold);

	// Radius padding (small safety margin)
	GLint locPad = glGetUniformLocation(m_lavaShader, "uRadiusPadding");
	if (locPad != -1) {
		float padding = glm::max(0.02f, 0.02f * m_lavaLamp.getRadius());
		glUniform1f(locPad, padding + 0.02f);
	}

	// Lighting
	vec3 lightPos = vec3(5.0f, 15.0f, 5.0f);
	vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
	vec3 ambientColor = vec3(0.2f, 0.1f, 0.1f);
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uLightPos"), 1, value_ptr(lightPos));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uLightColor"), 1, value_ptr(lightColor));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uAmbientColor"), 1, value_ptr(ambientColor));

	// Blob data
	auto positions = m_lavaLamp.getBlobPositions();
	auto radii = m_lavaLamp.getBlobRadii();
	auto blobbiness = m_lavaLamp.getBlobBlobbiness();
	auto colors = m_lavaLamp.getBlobColors();
	int blobCount = m_lavaLamp.getBlobCount();

	glUniform1i(glGetUniformLocation(m_lavaShader, "uBlobCount"), blobCount);
	if (blobCount > 0) {
		int count = std::min(blobCount, 16);
		glUniform4fv(glGetUniformLocation(m_lavaShader, "uBlobPositions"), count, value_ptr(positions[0]));
		glUniform1fv(glGetUniformLocation(m_lavaShader, "uBlobRadii"), count, radii.data());
		glUniform1fv(glGetUniformLocation(m_lavaShader, "uBlobBlobbiness"), count, blobbiness.data());
		glUniform3fv(glGetUniformLocation(m_lavaShader, "uBlobColors"), count, value_ptr(colors[0]));
	}

	// -------------------------
		// PASS 1: Depth-only pre-pass (metal + glass write depth)
		// -------------------------
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Render opaque metal (writes depth)
	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2);
	m_lampMetalMesh.draw();

	// -------------------------
	// PASS 2: Metaball raymarching (reads depth, writes color + depth)
	// -------------------------
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_LESS); // Pass if metaball <= existing depth
	glDepthMask(GL_TRUE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 1);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 1);

	// Bind depth texture from metal pre-pass (read-only)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureFront);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uDepthTexture"), 0);

	m_fullscreenQuad.draw();
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 0);

	// -------------------------
	// PASS 3: Metal color pass (opaque, replaces depth-only metal)
	// -------------------------
	glDepthFunc(GL_LEQUAL); // Draw over existing metal depth
	glDepthMask(GL_FALSE); // Don't modify depth

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2);
	m_lampMetalMesh.draw();

	// -------------------------
	// PASS 4: Glass color pass (transparent blending)
	// -------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0);
	m_lampGlassMesh.draw();

	// Restore state
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);

	// RESTORE ALL STATE at the end:
	glDepthMask(depthMask);
	glDepthFunc(depthFunc);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (!blendEnabled) {
		glDisable(GL_BLEND);
	}

	// IMPORTANT: Unbind textures and reset to default shader
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glUseProgram(0);


}

void Application::render() {
	// retrieve the window hieght
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	m_windowsize = vec2(width, height); // update window size
	glViewport(0, 0, width, height); // set the viewport to draw to the entire window

	// clear the back-buffer
	//glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // Darker background for better lava lamp visibility
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// pbr
	glUseProgram(m_pbr_shader);

	// projection matrix
	mat4 proj = perspective(1.f, float(1280) / float(720), 0.1f, 100.f);

	// model matrix
	mat4 model = glm::mat4(1.0f);

	// view matrix
	mat4 view = translate(mat4(1), vec3(0, -6, -m_distance))
		* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
		* rotate(mat4(1), m_yaw, vec3(0, 1, 0));

	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "projection"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "view"), 1, GL_FALSE, value_ptr(view));
	glUniform3fv(glGetUniformLocation(m_pbr_shader, "camPos"), 1, value_ptr(vec3(inverse(view) * vec4(0, 0, 0, 1))));

	// bind pre-computed IBL data
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);


	// gold
	bindPBRTextures(gold);
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0, 5.0, 0.0));
	model = glm::scale(model, glm::vec3(2.5, 2.5, 2.5));
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "model"), 1, GL_FALSE, value_ptr(model));
	glUniformMatrix3fv(glGetUniformLocation(m_pbr_shader, "normalMatrix"), 1, GL_FALSE, value_ptr(glm::transpose(glm::inverse(glm::mat3(model)))));
	//renderSphere();

	// render skybox
	glUseProgram(m_background_shader);
	mat4 viewSkybox = mat4(mat3(view));
	glUniformMatrix4fv(glGetUniformLocation(m_background_shader, "view"), 1, GL_FALSE, value_ptr(viewSkybox));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	renderCube();

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	// Render lava lamp
	renderLavaLamp(view, proj);

	renderTempCube(view, proj);


	// draw the original model (if desired)
	//m_model.draw(view, proj);
}

void Application::renderGUI() {
	// setup window
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiSetCond_Once);
	ImGui::Begin("Lava Lamp Controls", 0);

	// display current camera parameters
	ImGui::Text("Application %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::SliderFloat("Pitch", &m_pitch, -pi<float>() / 2, pi<float>() / 2, "%.2f");
	ImGui::SliderFloat("Yaw", &m_yaw, -pi<float>(), pi<float>(), "%.2f");
	ImGui::SliderFloat("Distance", &m_distance, 5, 50, "%.2f", 2.0f);

	// helpful drawing options
	ImGui::Checkbox("Show axis", &m_show_axis);
	ImGui::SameLine();
	ImGui::Checkbox("Show grid", &m_show_grid);
	ImGui::Checkbox("Wireframe", &m_showWireframe);
	ImGui::SameLine();
	if (ImGui::Button("Screenshot")) rgba_image::screenshot(true);

	ImGui::Separator();
	ImGui::Text("Lava Lamp Controls");
	ImGui::Checkbox("Show Lava Lamp", &m_showLavaLamp);
	ImGui::Checkbox("Animate", &m_animateLamp);

	// Lava lamp physics parameters
	if (ImGui::SliderFloat("Heater Temperature", &m_heaterTemp, 20.0f, 200.0f, "%.1f")) {
		m_lavaLamp.setHeaterTemperature(m_heaterTemp);
	}

	// Gravity is fixed now; don't expose a slider to avoid accidental changes.
	// m_gravity is permanently -9.8 (set in initializeLavaLamp)

	if (ImGui::SliderFloat("Blob Threshold", &m_threshold, 0.3f, 3.0f, "%.2f")) {
		m_lavaLamp.setThreshold(m_threshold);
	}

	ImGui::Separator();

	// Blob control buttons
	if (ImGui::Button("Add Blob")) {
		vec3 pos(0, 2, 0); // lower spawn so they get heated
		m_lavaLamp.addBlob(pos, 0.9f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Blob")) {
		m_lavaLamp.removeBlob();
	}

	if (ImGui::Button("Reset Lamp")) {
		m_lavaLamp.initialize(5);
	}

	ImGui::Text("Current blob count: %d", m_lavaLamp.getBlobCount());

	// finish creating window
	ImGui::End();
}

void Application::cursorPosCallback(double xpos, double ypos) {
	if (m_leftMouseDown) {
		vec2 whsize = m_windowsize / 2.0f;

		// clamp the pitch to [-pi/2, pi/2]
		m_pitch += float(acos(glm::clamp((m_mousePosition.y - whsize.y) / whsize.y, -1.0f, 1.0f))
			- acos(glm::clamp((float(ypos) - whsize.y) / whsize.y, -1.0f, 1.0f)));
		m_pitch = float(glm::clamp(m_pitch, -pi<float>() / 2, pi<float>() / 2));

		// wrap the yaw to [-pi, pi]
		m_yaw += float(acos(glm::clamp((m_mousePosition.x - whsize.x) / whsize.x, -1.0f, 1.0f))
			- acos(glm::clamp((float(xpos) - whsize.x) / whsize.x, -1.0f, 1.0f)));
		if (m_yaw > pi<float>())
			m_yaw -= float(2 * pi<float>());
		else if (m_yaw < -pi<float>())
			m_yaw += float(2 * pi<float>());
	}

	// updated mouse position
	m_mousePosition = vec2(xpos, ypos);
}

void Application::mouseButtonCallback(int button, int action, int mods) {
	(void)mods; // currently un-used

	// capture is left-mouse down
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		m_leftMouseDown = (action == GLFW_PRESS); // only other option is GLFW_RELEASE
}

void Application::scrollCallback(double xoffset, double yoffset) {
	(void)xoffset; // currently un-used
	m_distance *= pow(1.1f, -yoffset);
}

void Application::keyCallback(int key, int scancode, int action, int mods) {
	(void)key, (void)scancode, (void)action, (void)mods; // currently un-used
}

void Application::charCallback(unsigned int c) {
	(void)c; // currently un-used
}

void Application::loadPBRShaders() {
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
	mat4 projection = perspective(glm::radians(90.0f), float(1280) / float(720), 0.1f, 100.f);
	glUseProgram(m_pbr_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "projection"), 1, false, value_ptr(projection));
	glUseProgram(m_background_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_background_shader, "projection"), 1, false, value_ptr(projection));
}

GLuint Application::loadTexture(char const* path) {
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

textureData Application::loadPBRTextures(const std::string& basePath) {
	textureData tex;

	tex.albedo = loadTexture((basePath + "/albedo.png").c_str());
	tex.normal = loadTexture((basePath + "/normal.png").c_str());
	tex.metallic = loadTexture((basePath + "/metallic.png").c_str());
	tex.roughness = loadTexture((basePath + "/roughness.png").c_str());
	tex.ao = loadTexture((basePath + "/ao.png").c_str());

	return tex;
}

void Application::bindPBRTextures(const textureData& tex) {
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

float vertices[] = {
	// back face
	-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
	1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
	1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
	1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
	-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
	-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
	// front face
	-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
	1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
	1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
	1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
	-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
	-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
	// left face
	-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
	-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
	-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
	-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
	-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
	-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
	// right face
	1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
	1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
	1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
	1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
	1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
	1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
	// bottom face
	-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
	1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
	1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
	1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
	-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
	-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
	// top face
	-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
	1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
	1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
	1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
	-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
	-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
};

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void Application::renderCube() {
	// initialize (if necessary)
	if (cubeVAO == 0) {
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void Application::renderQuad() {
	if (quadVAO == 0) {
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

unsigned int sphereVAO = 0;
unsigned int indexCount;
void Application::renderSphere() {
	if (sphereVAO == 0) {
		glGenVertexArrays(1, &sphereVAO);

		unsigned int vbo, ebo;
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uv;
		std::vector<glm::vec3> normals;
		std::vector<unsigned int> indices;

		const unsigned int X_SEGMENTS = 64;
		const unsigned int Y_SEGMENTS = 64;
		const float PI = 3.14159265359f;
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
			for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
				float xSegment = (float)x / (float)X_SEGMENTS;
				float ySegment = (float)y / (float)Y_SEGMENTS;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

				positions.push_back(glm::vec3(xPos, yPos, zPos));
				uv.push_back(glm::vec2(xSegment, ySegment));
				normals.push_back(glm::vec3(xPos, yPos, zPos));
			}
		}

		bool oddRow = false;
		for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
			if (!oddRow) { // even rows: y == 0, y == 2; and so on 
				for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
					indices.push_back(y * (X_SEGMENTS + 1) + x);
					indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				}
			}
			else {
				for (int x = X_SEGMENTS; x >= 0; --x) {
					indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
					indices.push_back(y * (X_SEGMENTS + 1) + x);
				}
			}
			oddRow = !oddRow;
		}
		indexCount = static_cast<unsigned int>(indices.size());

		std::vector<float> data;
		for (unsigned int i = 0; i < positions.size(); ++i) {
			data.push_back(positions[i].x);
			data.push_back(positions[i].y);
			data.push_back(positions[i].z);
			if (normals.size() > 0) {
				data.push_back(normals[i].x);
				data.push_back(normals[i].y);
				data.push_back(normals[i].z);
			}
			if (uv.size() > 0) {
				data.push_back(uv[i].x);
				data.push_back(uv[i].y);
			}
		}
		glBindVertexArray(sphereVAO);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
		unsigned int stride = (3 + 2 + 3) * sizeof(float);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
	}
	glBindVertexArray(sphereVAO);
	glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
}