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
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(modelview));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	mesh.draw(); // draw
}

Application::Application(GLFWwindow* window) : m_window(window) {
	shader_builder sb;
	sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_vert.glsl"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_frag.glsl"));
	GLuint shader = sb.build();

	m_model.shader = shader;
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

	m_threshold = 0.2f;         // Lower threshold
	m_heaterTemp = 120.0f;      // increased heater for stronger rise
	m_gravity = -9.8f;          // fixed gravity (permanent)
	// m_viscosity intentionally not set: viscosity is now computed per-blob from temperature
	m_animSpeed = 1.0f;
	m_lavaLamp.setTimeScale(m_animSpeed);

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

void Application::renderLavaLamp(const glm::mat4& view, const glm::mat4& proj) {
	if (!m_showLavaLamp) return;

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
	// DEPTH PRE-PASS (two passes)
	// -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	// Disable color writes
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// FRONT faces -> m_depthTextureFront
	if (m_depthTextureFront != 0) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextureFront, 0);
	}
	else {
		std::cerr << "Warning: m_depthTextureFront == 0 (front depth texture missing)\n";
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Cull back faces so we capture front surface depth
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2); // Metal
	m_lampMetalMesh.draw();
	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0); // Glass
	m_lampGlassMesh.draw();

	// BACK faces -> m_depthTextureBack
	if (m_depthTextureBack != 0) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTextureBack, 0);
	}
	else {
		std::cerr << "Warning: m_depthTextureBack == 0 (back depth texture missing)\n";
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}
	glClear(GL_DEPTH_BUFFER_BIT);

	// Cull front faces so we capture back surface depth
	glCullFace(GL_FRONT);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2); // Metal
	m_lampMetalMesh.draw();
	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0); // Glass
	m_lampGlassMesh.draw();

	// Done with depth passes
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Optionally check completeness once for debugging
	if (m_depthFBO != 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_depthFBO);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::cerr << "Depth FBO incomplete: 0x" << std::hex << status << std::dec << "\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Restore viewport
	glViewport(0, 0, width, height);

	// -------------------------
	// METABALL PASS (raymarch)
	// - create a small 1x1 fallback texture once so we never bind texture 0 to a unit
	// -------------------------
	static GLuint s_fallbackTex = 0;
	if (s_fallbackTex == 0) {
		glGenTextures(1, &s_fallbackTex);
		glBindTexture(GL_TEXTURE_2D, s_fallbackTex);
		// make a tiny single-channel float texture with value 1.0 (depth far)
		float v = 1.0f;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &v);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// ensure base/max level set so driver is happy
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Determine which textures are available
	bool haveFront = (m_depthTextureFront != 0);
	bool haveBack = (m_depthTextureBack != 0);
	bool haveBothDepths = haveFront && haveBack;

	// Bind front depth (or fallback) to unit 0, set sampler uniform to 0
	glActiveTexture(GL_TEXTURE0);
	if (haveFront) {
		glBindTexture(GL_TEXTURE_2D, m_depthTextureFront);
		// ensure sampling returns float not compare
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, s_fallbackTex);
	}
	GLint locDepthFront = glGetUniformLocation(m_lavaShader, "uDepthFrontTex");
	if (locDepthFront != -1) glUniform1i(locDepthFront, 0);

	// Bind back depth (or fallback) to unit 1, set sampler uniform to 1
	glActiveTexture(GL_TEXTURE1);
	if (haveBack) {
		glBindTexture(GL_TEXTURE_2D, m_depthTextureBack);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, s_fallbackTex);
	}
	GLint locDepthBack = glGetUniformLocation(m_lavaShader, "uDepthBackTex");
	if (locDepthBack != -1) glUniform1i(locDepthBack, 1);

	// Tell shader whether real depths are available
	GLint locHaveDepth = glGetUniformLocation(m_lavaShader, "uHaveDepthTex");
	if (locHaveDepth != -1) glUniform1i(locHaveDepth, haveBothDepths ? 1 : 0);

	// Fullscreen quad flag
	GLint locIsFullscreen = glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad");
	if (locIsFullscreen != -1) glUniform1i(locIsFullscreen, 1);

	// Draw metaballs on fullscreen quad
	glDisable(GL_DEPTH_TEST); // raymarch will use depth textures for occlusion
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 1); // Metaballs
	m_fullscreenQuad.draw();

	// cleanup metaball pass state
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	if (locIsFullscreen != -1) glUniform1i(locIsFullscreen, 0);

	// Unbind the texture units (bind fallback or 0 if you prefer)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, s_fallbackTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_fallbackTex);

	// -------------------------
	// COLOR PASSES (metal then glass)
	// -------------------------
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2); // Metal
	m_lampMetalMesh.draw();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0); // Glass
	m_lampGlassMesh.draw();

	// Restore state
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void Application::render() {
	// retrieve the window hieght
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	m_windowsize = vec2(width, height); // update window size
	glViewport(0, 0, width, height); // set the viewport to draw to the entire window

	// clear the back-buffer
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // Darker background for better lava lamp visibility
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// enable flags for normal/forward rendering
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// projection matrix
	mat4 proj = perspective(1.f, float(width) / height, 0.1f, 1000.f);

	// view matrix
	mat4 view = translate(mat4(1), vec3(0, -6, -m_distance))
		* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
		* rotate(mat4(1), m_yaw, vec3(0, 1, 0));

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	// Render lava lamp
	renderLavaLamp(view, proj);

	// draw the original model (if desired)
	// m_model.draw(view, proj);
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

	// Animation speed replaces the old viscosity UI (viscosity is now temperature-driven)
	if (ImGui::SliderFloat("Animation Speed", &m_animSpeed, 0.1f, 4.0f, "%.2f")) {
		// set the simulator time scale (1.0 = real time)
		m_lavaLamp.setTimeScale(m_animSpeed);
	}
	ImGui::Text("Animation Speed: x%.2f", m_animSpeed);

	// Lava lamp physics parameters
	if (ImGui::SliderFloat("Heater Temperature", &m_heaterTemp, 20.0f, 200.0f, "%.1f")) {
		m_lavaLamp.setHeaterTemperature(m_heaterTemp);
	}

	// Gravity is fixed now; don't expose a slider to avoid accidental changes.
	// m_gravity is permanently -9.8 (set in initializeLavaLamp)

	if (ImGui::SliderFloat("Blob Threshold", &m_threshold, 0.05f, 1.0f, "%.2f")) {
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