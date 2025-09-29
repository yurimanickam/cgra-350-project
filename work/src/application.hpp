#pragma once

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"
#include "skeleton_model.hpp"
#include "lava_lamp.hpp" // Add this include

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
	cgra::gl_mesh m_lampGlassMesh;
	cgra::gl_mesh m_lampMetalMesh;
	cgra::gl_mesh m_fullscreenQuad; // Add fullscreen quad for raymarching

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
	float m_animSpeed = 1.0f;

	// Helper methods for lava lamp
	void initializeLavaLamp();
	void renderLavaLamp(const glm::mat4& view, const glm::mat4& proj);
	cgra::gl_mesh createLampContainerGlass();
	cgra::gl_mesh createLampContainerMetal();
	cgra::gl_mesh createFullscreenQuad(); // Add method for fullscreen quad

	// Depth FBO helper
	void ensureDepthFBO(int width, int height);

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