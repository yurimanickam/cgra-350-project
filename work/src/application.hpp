#pragma once

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// std
#include <map>
#include <vector>
#include <string>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"
#include "yuri/objloader.hpp"

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
	glm::vec3 color{ 0.7f };
	glm::mat4 modelTransform{ 1.0 };
	GLuint texture;

	void draw(const glm::mat4& view, const glm::mat4 proj);
};

// PBR material structure - simple wrapper around the PBR material indices
struct pbr_material_wrapper {
	int material_index; // Index into available materials (0=gold, 1=plastic, 2=cloth, etc.)
	std::string name;
};

// Main application class
//
class Application {
private:
	// window
	glm::vec2 m_windowsize;
	GLFWwindow* m_window;

	// oribital camera
	float m_pitch = -0.5f;
	float m_yaw = -0.0f;
	float m_distance = 20;

	// Camera movement
	glm::vec3 m_cameraPos{ 0.0f, 20.0f, 20.0f };
	float m_cameraSpeed = 10.0f;
	bool m_invertMouseY = true;
	bool m_moveForward = false;
	bool m_moveBackward = false;
	bool m_moveLeft = false;
	bool m_moveRight = false;
	bool m_moveUp = false;
	bool m_moveDown = false;

	// last input
	bool m_leftMouseDown = false;
	glm::vec2 m_mousePosition;

	// drawing flags
	bool m_show_axis = false;
	bool m_show_grid = false;
	bool m_showWireframe = false;
	bool m_show_model = false;


	// geometry - modified to support multi-material model
	cgra::multi_mesh_model m_multiModel;
	std::map<std::string, pbr_material_wrapper> m_material_assignments;
	bool m_useMultiMaterial = true;

	// Keep the old single model for compatibility
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
	bool m_showLavaLamp = false;
	bool m_animateLamp = false;


	bool m_UseSkybox = false;
	bool m_UseSphere = false;

	Station m_station;
	basic_model m_cylinderModel;
	bool m_drawCylinder = true;

	void assignRandomPBRMaterials();
	void bindPBRMaterialByIndex(int materialIndex);

public:
	// setup
	Application(GLFWwindow*);
	void updateCameraMovement(float deltaTime);

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