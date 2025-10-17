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

#include "matt/render_utils.hpp"
#include "matt/pbr.hpp"

using namespace std;
using namespace cgra;
using namespace glm;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

void basic_model::draw(const glm::mat4& view, const glm::mat4 proj) {
	mat4 modelview = view * modelTransform;

	glUseProgram(shader); // load shader and variables
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelview));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	mesh.draw(); // draw
}


Application::Application(GLFWwindow* window) : m_window(window) {
	buildShaders();

	m_shader = m_default_shader;
	m_model.shader = m_shader;
	//m_model.mesh = load_wavefront_data(CGRA_SRCDIR + std::string("/res//assets//teapot.obj")).build();
	m_model.color = vec3(1, 0, 0);

	// Initialize lava lamp using new LavaLamp API
	m_lavaLamp.initialiseLavaLamp(
		CGRA_SRCDIR + std::string("//res//shaders//lava_vertex.glsl"),
		CGRA_SRCDIR + std::string("//res//shaders//lava_fragment.glsl")
	);
}

cgra::gl_mesh Application::createLampContainerMetal() {
	mesh_builder builder;
	const int segments = 64;

	// Lower third of bulb (metal)
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

	// Glass bottom cap (inverted)
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

	// Glass top cap
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

	// Metal base
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

	// PASS 1: Metaball raymarching
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 1);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	m_fullscreenQuadModel.draw(view, proj);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 0);


	// PASS 2: Glass
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LESS);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0);
	m_lampGlassModel.draw(view, proj);


	// PASS 3: Metal with PBR
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	// Switch to PBR shader for metal parts
	glUseProgram(m_pbr_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "projection"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "view"), 1, GL_FALSE, value_ptr(view));
	glUniform3fv(glGetUniformLocation(m_pbr_shader, "camPos"), 1, value_ptr(cameraPos));

	// Bind IBL data
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

	// Bind gold PBR textures
	bindPBRTextures(gold);

	// Set model matrix for lamp metal
	mat4 metalModel = mat4(1.0f);
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "model"), 1, GL_FALSE, value_ptr(metalModel));
	glUniformMatrix3fv(glGetUniformLocation(m_pbr_shader, "normalMatrix"), 1, GL_FALSE, value_ptr(glm::transpose(glm::inverse(glm::mat3(metalModel)))));

	// Draw metal parts with PBR shader
	m_lampMetalModel.mesh.draw();

	// Switch back to lava shader
	glUseProgram(m_lavaShader);

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

	float currentFrame = glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// clear the back-buffer
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // Darker background for better lava lamp visibility
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// projection matrix
	mat4 proj = perspective(1.f, float(1280) / float(720), 0.1f, 100.f);

	// model matrix
	mat4 model = glm::mat4(1.0f);

	// view matrix
	mat4 view = translate(mat4(1), vec3(0, -6, -m_distance))
		* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
		* rotate(mat4(1), m_yaw, vec3(0, 1, 0));

	if (m_UseSkybox || m_UseSphere) {
		// pbr
		glUseProgram(m_pbr_shader);
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
	}
	else {
		glUseProgram(m_default_shader);
	}

	if (m_UseSphere) {
		// gold
		bindPBRTextures(gold);
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0, 5.0, 0.0));
		model = glm::scale(model, glm::vec3(2.5, 2.5, 2.5));
		glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "model"), 1, GL_FALSE, value_ptr(model));
		glUniformMatrix3fv(glGetUniformLocation(m_pbr_shader, "normalMatrix"), 1, GL_FALSE, value_ptr(glm::transpose(glm::inverse(glm::mat3(model)))));
		renderSphere();

		// plastic
		bindPBRTextures(plastic);
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(5.5, 5.0, 0.0));
		model = glm::scale(model, glm::vec3(2.5, 2.5, 2.5));
		glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "model"), 1, GL_FALSE, value_ptr(model));
		glUniformMatrix3fv(glGetUniformLocation(m_pbr_shader, "normalMatrix"), 1, GL_FALSE, value_ptr(glm::transpose(glm::inverse(glm::mat3(model)))));
		renderSphere();

		bindPBRTextures(cloth);
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-5.5, 5.0, 0.0));
		model = glm::scale(model, glm::vec3(2.5, 2.5, 2.5));
		glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "model"), 1, GL_FALSE, value_ptr(model));
		glUniformMatrix3fv(glGetUniformLocation(m_pbr_shader, "normalMatrix"), 1, GL_FALSE, value_ptr(glm::transpose(glm::inverse(glm::mat3(model)))));
		renderSphere();
	}
	if (m_UseSkybox) {
		// render skybox
		glUseProgram(m_background_shader);
		mat4 viewSkybox = mat4(mat3(view));
		glUniformMatrix4fv(glGetUniformLocation(m_background_shader, "view"), 1, GL_FALSE, value_ptr(viewSkybox));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
		renderCube();
	}

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	// Render lava lamp
	m_lavaLamp.renderLavaLamp(
		view, proj, m_window,
		m_animateLamp, m_showLavaLamp, m_threshold,
		m_heaterTemp, m_gravity
	);

	// draw the original model (if desired)
	//m_model.draw(view, proj);
}

void Application::renderGUI() {
	// setup window
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiSetCond_Once);
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

	// In Application::renderGUI(), replace the Space Station section
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(410, 5), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiSetCond_Once);
	ImGui::Begin("PBR Controls", 0);

	ImGui::Text("Physically Based Rendering (PBR) Settings");
	ImGui::Checkbox("Use Skybox", &m_UseSkybox);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Sphere", &m_UseSphere);

	ImGui::Separator();

	ImGui::Text("Change IBL Environment");
	
	if (ImGui::Button("Space Environment")) {
		loadPBRShaders(CGRA_SRCDIR + std::string("//res//textures//space.hdr"));
	}

	if (ImGui::Button("Studio Environment")) {
		loadPBRShaders(CGRA_SRCDIR + std::string("//res//textures//studio.hdr"));
	}

	if (ImGui::Button("Sunset Environment")) {
		loadPBRShaders(CGRA_SRCDIR + std::string("//res//textures//sunset.hdr"));
	}
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