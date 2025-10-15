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


	// ----------------------------
	// Space station cubes (now PBR gold material)
	// ----------------------------
	static std::vector<BoundCube> spaceStationCubes;
	static std::vector<StationModule> spaceStationModules;
	static bool cubesInitialized = false;
	static bool stationInitialized = false;
	static float lastSphereRadius = 10.0f;

	// Track last parameters for change detection
	static int lastIterations = -1;
	static float lastLengthScale = -1.0f;
	static float lastRadiusScale = -1.0f;
	static float lastBranchAngle = -1.0f;
	static float lastBranchProbability = -1.0f;
	static float lastMainLength = -1.0f;
	static float lastMainRadius = -1.0f;
	static unsigned int lastRandomSeed = 0;

	// Check if any parameter changed
	bool paramsChanged = (m_regenerateStation ||
		!stationInitialized ||
		lastIterations != m_stationIterations ||
		std::abs(lastLengthScale - m_stationLengthScale) > 0.001f ||
		std::abs(lastRadiusScale - m_stationRadiusScale) > 0.001f ||
		std::abs(lastBranchAngle - m_stationBranchAngle) > 0.001f ||
		std::abs(lastBranchProbability - m_stationBranchProbability) > 0.001f ||
		std::abs(lastMainLength - m_stationMainLength) > 0.001f ||
		std::abs(lastMainRadius - m_stationMainRadius) > 0.001f ||
		lastRandomSeed != m_stationRandomSeed);

	// Regenerate if requested or parameters changed
	if (paramsChanged) {
		// Clean up old station modules
		for (auto& module : spaceStationModules) {
			if (module.vao != 0) {
				glDeleteVertexArrays(1, &module.vao);
				glDeleteBuffers(1, &module.vbo);
				if (module.ebo != 0) {
					glDeleteBuffers(1, &module.ebo);
				}
			}
		}
		spaceStationModules.clear();

		// Update random seed if auto-randomize is enabled
		if (m_autoRandomSeed && m_regenerateStation) {
			m_stationRandomSeed = static_cast<unsigned>(std::time(nullptr));
		}

		// Generate custom parameters based on current sliders
		LSystemParams params = createCustomStationParams(
			m_stationIterations,
			m_stationLengthScale,
			m_stationRadiusScale,
			m_stationBranchAngle,
			m_stationBranchProbability,
			m_stationRandomSeed
		);

		// Generate new procedural L-system space station
		spaceStationModules = generateProceduralStation(
			params,
			m_stationMainLength,
			m_stationMainRadius
		);

		stationInitialized = true;
		m_regenerateStation = false;

		// Update last known parameters
		lastIterations = m_stationIterations;
		lastLengthScale = m_stationLengthScale;
		lastRadiusScale = m_stationRadiusScale;
		lastBranchAngle = m_stationBranchAngle;
		lastBranchProbability = m_stationBranchProbability;
		lastMainLength = m_stationMainLength;
		lastMainRadius = m_stationMainRadius;
		lastRandomSeed = m_stationRandomSeed;
	}

	// Use PBR shader and bind required textures
	glUseProgram(m_pbr_shader);
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "projection"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(m_pbr_shader, "view"), 1, GL_FALSE, value_ptr(view));
	glUniform3fv(glGetUniformLocation(m_pbr_shader, "camPos"), 1, value_ptr(vec3(inverse(view) * vec4(0, 0, 0, 1))));

	// Bind IBL data
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

	// Bind gold PBR textures (albedo/normal/metallic/roughness/ao)
	bindPBRTextures(gold);

	// Render L-system procedural space station
	renderStationModulesPBR(spaceStationModules, view, proj, m_pbr_shader);

	// Generate greebles for each module (do this after station generation)
	static std::vector<Greeble> allGreebles;
	// Regenerate greebles when station changes OR when greeble parameters change
	static float lastScaleFactor = 1.0f;
	static float lastScaleProportion = 0.0f;
	static float lastScaleMix = 0.0f;
	static int lastGreebleCount = -1;

	bool greebleParamsChanged = (lastScaleFactor != m_greebleScaleFactor ||
		lastScaleProportion != m_greebleScaleProportion ||
		lastScaleMix != m_greebleScaleMix ||
		lastGreebleCount != m_greebleCountPerModule);

	if (paramsChanged || !m_greeblesGenerated || greebleParamsChanged) {
		allGreebles.clear();

		for (size_t i = 0; i < spaceStationModules.size(); ++i) {
			auto moduleGreebles = generateGreeblesForModule(
				spaceStationModules[i],
				m_greebleCountPerModule,
				m_stationRandomSeed + static_cast<unsigned>(i),
				m_greebleScaleFactor,
				m_greebleScaleProportion,
				m_greebleScaleMix  // Pass the new parameter
			);
			allGreebles.insert(allGreebles.end(), moduleGreebles.begin(), moduleGreebles.end());
		}

		m_greeblesGenerated = true;
		lastScaleFactor = m_greebleScaleFactor;
		lastScaleProportion = m_greebleScaleProportion;
		lastScaleMix = m_greebleScaleMix;
		lastGreebleCount = m_greebleCountPerModule;
		std::cout << "Generated " << allGreebles.size() << " total greebles" << std::endl;
	}

	renderGreeblesPBR(allGreebles, view, proj, m_pbr_shader);


	// Optionally render legacy cubes
	if (m_showLegacyCubes) {
		renderBoundCubesPBR(spaceStationCubes, view, proj, m_pbr_shader);
	}

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

	// In Application::renderGUI(), replace the Space Station section:

	ImGui::Separator();
	ImGui::Text("Space Station L-System Controls");

	if (ImGui::Button("Minimal Preset")) {
		LSystemParams preset = createMinimalStationParams();
		m_stationIterations = preset.iterations;
		m_stationLengthScale = preset.lengthScale;
		m_stationRadiusScale = preset.radiusScale;
		m_stationBranchAngle = preset.branchAngle;
		m_stationBranchProbability = preset.branchProbability; // NEW
		m_stationRandomSeed = preset.randomSeed;
	}
	ImGui::SameLine();
	if (ImGui::Button("Standard Preset")) {
		LSystemParams preset = createStandardStationParams();
		m_stationIterations = preset.iterations;
		m_stationLengthScale = preset.lengthScale;
		m_stationRadiusScale = preset.radiusScale;
		m_stationBranchAngle = preset.branchAngle;
		m_stationBranchProbability = preset.branchProbability; // NEW
		m_stationRandomSeed = preset.randomSeed;
	}
	ImGui::SameLine();
	if (ImGui::Button("Complex Preset")) {
		LSystemParams preset = createComplexStationParams();
		m_stationIterations = preset.iterations;
		m_stationLengthScale = preset.lengthScale;
		m_stationRadiusScale = preset.radiusScale;
		m_stationBranchAngle = preset.branchAngle;
		m_stationBranchProbability = preset.branchProbability; // NEW
		m_stationRandomSeed = preset.randomSeed;
	}

	ImGui::Spacing();

	// L-System Generation Parameters
	if (ImGui::SliderInt("Iterations", &m_stationIterations, 1, 5)) {
		// Automatic regeneration on change
	}

	if (ImGui::SliderFloat("Length Scale", &m_stationLengthScale, 0.3f, 1.0f, "%.2f")) {
		// Automatic regeneration on change
	}

	if (ImGui::SliderFloat("Width Scale", &m_stationRadiusScale, 0.3f, 1.0f, "%.2f")) {
		// Automatic regeneration on change
	}

	if (ImGui::SliderFloat("Branch Angle", &m_stationBranchAngle, 30.0f, 120.0f, "%.1f°")) {
		// Automatic regeneration on change
	}

	// NEW: Add this slider
	if (ImGui::SliderFloat("Branch Probability", &m_stationBranchProbability, 0.0f, 1.0f, "%.2f")) {
		// Automatic regeneration on change
	}
	ImGui::SameLine();
	if (ImGui::Button("?##branchprob")) {}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Controls how likely secondary branches are to produce children.\n"
			"1.0 = symmetric (all branches branch)\n"
			"0.5 = moderate asymmetry\n"
			"0.0 = no secondary branching");
	}

	ImGui::Spacing();

	// Module Dimensions
	if (ImGui::SliderFloat("Main Length", &m_stationMainLength, 2.0f, 15.0f, "%.1f")) {
		// Automatic regeneration on change
	}

	if (ImGui::SliderFloat("Main Width", &m_stationMainRadius, 0.5f, 3.0f, "%.1f")) {
		// Automatic regeneration on change
	}

	ImGui::Spacing();

	// Randomization Controls
	ImGui::Checkbox("Auto-Randomize Seed", &m_autoRandomSeed);

	if (!m_autoRandomSeed) {
		if (ImGui::SliderInt("Random Seed", reinterpret_cast<int*>(&m_stationRandomSeed), 0, 10000)) {
			// Automatic regeneration on change
		}
	}

	if (ImGui::Button("Regenerate Station")) {
		m_regenerateStation = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("New Random Seed")) {
		m_stationRandomSeed = static_cast<unsigned>(std::time(nullptr));
	}
	ImGui::Spacing();
	if (ImGui::SliderInt("Greebles Per Module", &m_greebleCountPerModule, 0, 50)) {
		m_greeblesGenerated = false; // Force regeneration
	}

	if (ImGui::SliderFloat("Greeble Scale Factor", &m_greebleScaleFactor, 0.5f, 10.0f, "%.2f")) {
		m_greeblesGenerated = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("?##scalefactor")) {}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("How much to scale affected greebles.\n"
			"1.0 = normal size, 5.0 = 5x larger");
	}

	if (ImGui::SliderFloat("Scale Proportion", &m_greebleScaleProportion, 0.0f, 1.0f, "%.2f")) {
		m_greeblesGenerated = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("?##scaleprop")) {}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Proportion of greebles randomly affected by scaling.\n"
			"0.0 = none, 0.3 = 30%, 1.0 = all");
	}

	if (ImGui::SliderFloat("Scale Direction Mix", &m_greebleScaleMix, 0.0f, 1.0f, "%.2f")) {
		m_greeblesGenerated = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("?##scalemix")) {}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Blend between scaling modes:\n"
			"0.0 = Uniform (all directions)\n"
			"0.5 = Mixed\n"
			"1.0 = Normal only (solar panel mode)");
	}

	ImGui::Spacing();

	ImGui::Checkbox("Show Legacy Cubes", &m_showLegacyCubes);

	if (m_showLegacyCubes) {
		if (ImGui::SliderFloat("Cube Sphere Radius", &m_stationSphereRadius, 1.0f, 30.0f, "%.1f")) {
			// Radius changed
		}
	}

	ImGui::Spacing();
	ImGui::TextWrapped("Tip: Adjust sliders in real-time to see changes. Auto-regeneration enabled!");

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