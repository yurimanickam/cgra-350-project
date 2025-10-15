#pragma once

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"
#include "skeleton_model.hpp"

//teammate includes
#include "david/lava_lamp.hpp"
#include "yuri/station.hpp"
#include "matt/pbr.hpp"

// Basic model that holds the shader, mesh and transform for drawing.
// Can be copied and modified for adding in extra information for drawing
// including textures for texture mapping etc.
struct basic_model {
	GLuint shader = 0;
	cgra::gl_mesh mesh;
	glm::vec3 color{ 0.7 };
	glm::mat4 modelTransform{ 1.0 };
	GLuint texture;

	void draw(const glm::mat4& view, const glm::mat4 proj);
};

// Main application class
//
class Application {
private:
	// window
	glm::vec2 m_windowsize;
	GLFWwindow* m_window;

	// oribital camera
	float m_pitch = .86;
	float m_yaw = -.86;
	float m_distance = 20;

	// last input
	bool m_leftMouseDown = false;
	glm::vec2 m_mousePosition;

	// drawing flags
	bool m_show_axis = false;
	bool m_show_grid = false;
	bool m_showWireframe = false;

	// geometry
	basic_model m_model;

	// Lava lamp components
	LavaLamp m_lavaLamp;
	GLuint m_lavaShader = 0;
	basic_model m_lampGlassModel;
	basic_model m_lampMetalModel;
	basic_model m_fullscreenQuadModel; // fullscreen quad for raymarching

	GLuint m_depthFBO = 0;
	GLuint m_depthTextureFront = 0; // depth from front faces
	GLuint m_depthTextureBack = 0;  // depth from back faces
	int m_depthTexW = 0;
	int m_depthTexH = 0;

	// Animation timing
	float m_lastTime = 0.0f;

	// Lava lamp parameters for GUI
	float m_heaterTemp = 100.0f;
	float m_gravity = -9.8f;
	float m_viscosity = 0.3f;
	float m_threshold = 0.2f;
	bool m_showLavaLamp = true;
	bool m_animateLamp = true;

	// Helper methods for lava lamp
	void initializeLavaLamp();
	void renderLavaLamp(const glm::mat4& view, const glm::mat4& proj);
	cgra::gl_mesh createLampContainerGlass();
	cgra::gl_mesh createLampContainerMetal();
	cgra::gl_mesh createFullscreenQuad(); // Add method for fullscreen quad

	// Depth FBO helper
	void ensureDepthFBO(int width, int height);

	bool m_UseSkybox = true;
	bool m_UseSphere = false;

	// Space Station parameters
	int m_stationComplexity = 2; // 1=minimal, 2=standard, 3=complex
	bool m_regenerateStation = true;
	bool m_showLegacyCubes = false;
	float m_stationSphereRadius = 10.0f;

	int m_stationIterations = 3;
	float m_stationLengthScale = 0.7f;
	float m_stationRadiusScale = 0.75f;
	float m_stationBranchAngle = 90.0f;
	float m_stationBranchProbability = 0.8f; // NEW: Add this
	float m_stationMainLength = 8.0f;
	float m_stationMainRadius = 1.5f;
	unsigned int m_stationRandomSeed = 0;
	bool m_autoRandomSeed = false;  // ADD THIS LINE

	// In Application class definition
	int m_greebleCountPerModule = 15;
	bool m_greeblesGenerated = false;



	// Greeble scaling controls
	float m_greebleScaleFactor = 1.0f;
	float m_greebleScaleProportion = 0.0f;
	float m_greebleScaleMix = 0.0f; // 0.0 = uniform scaling, 1.0 = normal-only scaling
	
public:
	// setup
	Application(GLFWwindow*);

	// disable copy constructors (for safety)
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	// rendering callbacks (every frame)
	void render();
	void renderGUI();

	// input callbacks
	void cursorPosCallback(double xpos, double ypos);
	void mouseButtonCallback(int button, int action, int mods);
	void scrollCallback(double xoffset, double yoffset);
	void keyCallback(int key, int scancode, int action, int mods);
	void charCallback(unsigned int c);
};