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


void Application::render() {
	// retrieve the window hieght
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	m_windowsize = vec2(width, height); // update window size
	glViewport(0, 0, width, height); // set the viewport to draw to the entire window

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

		// gold
		bindPBRTextures(plastic);
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(1.5, 0.5, 0.0));
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
	ImGui::SetNextWindowSize(ImVec2(410, 650), ImGuiSetCond_Once);
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
	ImGui::Checkbox("Use Skybox", &m_UseSkybox);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Sphere", &m_UseSphere);

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