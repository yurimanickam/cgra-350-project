#pragma once

// glm
#include <glm/glm.hpp>

// std
#include <vector>
#include <random>

// project
#include "cgra/cgra_mesh.hpp"

struct LavaBlob {
	glm::vec3 position;
	glm::vec3 velocity;
	float radius;
	float temperature;
	float blobbiness;
	glm::vec3 color;

	// Spring-based physics
	glm::vec3 anchorPoint;       // Virtual point this blob is attracted to
	float anchorStrength = 1.0f; // Spring constant to anchor
	float heatPhase = 0.0f;      // 0-1 cycle position (0=bottom, 0.5=top, 1=bottom)
	float cycleSpeed = 1.0f;     // How fast it cycles

	LavaBlob(glm::vec3 pos = glm::vec3(0.0f), float r = 1.0f);
};


// Main lava lamp simulation class
class LavaLamp {
private:
	std::vector<LavaBlob> m_blobs;

	float m_springConstant = 3.0f;       // Strength of attraction to anchor
	float m_dampingConstant = 5.0f;      // Velocity damping
	float m_repulsionStrength = 2.0f;    // Inter-blob repulsion
	float m_repulsionRange = 1.5f;       // Range of repulsion (in radii)

	float m_radius = 1.8f;      // max bulb radius (matches lamp mesh max)
	float m_height = 10.0f;     // lamp height (matches mesh)
	float m_baseHeight = 2.0f;  // Heating element height (glass bottom height)

	// Physics parameters
	float m_gravity = -9.8f;
	float m_heatDiffusion = 0.1f;
	float m_ambientTemp = 20.0f;
	float m_heaterTemp = 80.0f;

	// Metaball parameters
	float m_threshold = 0.5f;         // Isosurface threshold
	int m_gridResolution = 32;        // Marching cubes grid resolution (if used)

	// Random number generation
	std::mt19937 m_rng;
	std::uniform_real_distribution<float> m_randomDist;

	// Helper functions (simulation internals)
	void updateBlobPhysics(LavaBlob& blob, float dt);
	void handleBlobInteractions();
	void applyBoundaryConditions(LavaBlob& blob);
	float computeDensityField(const glm::vec3& point) const;
	glm::vec3 computeDensityGradient(const glm::vec3& point) const;

	void updateAnchorPoints(float dt);
	glm::vec3 computeRepulsionForce(const LavaBlob& blob, size_t blobIndex);

	// Mesh generation (if you use marching cubes fallback)
	cgra::gl_mesh generateMarchingCubesMesh();
	float sampleField(int x, int y, int z);
	glm::vec3 interpolateVertex(const glm::vec3& p1, const glm::vec3& p2, float v1, float v2);

public:
	LavaLamp();
	~LavaLamp();

	// Initialize with number of blobs (default 5)
	void initialize(int numBlobs = 5);

	// Update simulation by deltaTime (seconds)
	void update(float deltaTime);

	// Get mesh for rendering (empty if using shader raymarching)
	cgra::gl_mesh getMesh();

	// Getters for shader uniforms / renderer
	std::vector<glm::vec4> getBlobPositions() const;
	std::vector<float> getBlobRadii() const;
	std::vector<float> getBlobBlobbiness() const;
	std::vector<glm::vec3> getBlobColors() const;
	int getBlobCount() const { return static_cast<int>(m_blobs.size()); }

	// Expose lamp geometry so app/shader use same volume
	float getRadius() const { return m_radius; }
	float getHeight() const { return m_height; }
	float getBaseHeight() const { return m_baseHeight; }

	// Control parameters (external UI/app)
	void setGravity(float g) { m_gravity = g; }
	void setHeaterTemperature(float t) { m_heaterTemp = t; }
	void setThreshold(float t) { m_threshold = t; }

	// Add/remove blobs
	void addBlob(const glm::vec3& position, float radius);
	void removeBlob();

	void mergeBlobsIfClose();
	void splitLargeBlobs();
};
