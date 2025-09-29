#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <random>
#include "cgra/cgra_mesh.hpp"

// Represents a single blob in the lava lamp
struct LavaBlob {
    glm::vec3 position;
    glm::vec3 velocity;
    float radius;
    float temperature;  // Affects buoyancy
    float blobbiness;   // Controls blob shape (-1 to 0, negative for proper decay)
    glm::vec3 color;

    LavaBlob(glm::vec3 pos = glm::vec3(0), float r = 1.0f);
};

// Main lava lamp simulation class
class LavaLamp {
private:
    std::vector<LavaBlob> m_blobs;

    // Lamp dimensions — MATCH THESE TO YOUR MESH
    float m_radius = 1.8f;      // max bulb radius (matches lamp mesh max)
    float m_height = 10.0f;     // lamp height (matches mesh)
    float m_baseHeight = 2.0f;  // Heating element height

    // Physics parameters
    float m_gravity = -9.8f;
    float m_viscosity = 0.5f;
    float m_heatDiffusion = 0.1f;
    float m_ambientTemp = 20.0f;
    float m_heaterTemp = 80.0f;

    // Metaball parameters
    float m_threshold = 0.5f;    // Isosurface threshold
    int m_gridResolution = 32;   // Marching cubes grid resolution

    // Random number generation
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_randomDist;

    // Helper functions
    void updateBlobPhysics(LavaBlob& blob, float dt);
    void handleBlobInteractions();
    void applyBoundaryConditions(LavaBlob& blob);
    float computeDensityField(const glm::vec3& point) const;
    glm::vec3 computeDensityGradient(const glm::vec3& point) const;

    // Mesh generation
    cgra::gl_mesh generateMarchingCubesMesh();
    float sampleField(int x, int y, int z);
    glm::vec3 interpolateVertex(const glm::vec3& p1, const glm::vec3& p2, float v1, float v2);

public:
    LavaLamp();
    ~LavaLamp();

    // Initialize with number of blobs
    void initialize(int numBlobs = 5);

    // Update simulation
    void update(float deltaTime);

    // Get mesh for rendering
    cgra::gl_mesh getMesh();

    // Getters for shader uniforms
    std::vector<glm::vec4> getBlobPositions() const;
    std::vector<float> getBlobRadii() const;
    std::vector<float> getBlobBlobbiness() const;
    std::vector<glm::vec3> getBlobColors() const;
    int getBlobCount() const { return m_blobs.size(); }

    // Expose lamp geometry so app/shader use same volume
    float getRadius() const { return m_radius; }
    float getHeight() const { return m_height; }
    float getBaseHeight() const { return m_baseHeight; }

    // Control parameters
    void setGravity(float g) { m_gravity = g; }
    void setViscosity(float v) { m_viscosity = v; }
    void setHeaterTemperature(float t) { m_heaterTemp = t; }
    void setThreshold(float t) { m_threshold = t; }

    // Add/remove blobs
    void addBlob(const glm::vec3& position, float radius);
    void removeBlob();
    void mergeBlobsIfClose();
    void splitLargeBlobs();
};
