#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <iostream>
#include <string>

// Holds mesh and transform for each cylinder module
struct StationModule {
    unsigned int vao = 0, vbo = 0, ebo = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 color = glm::vec3(0.2, 0.7, 1.0);
    float length = 1.0f;
    float radius = 5.0f;
    int generation = 0;
    unsigned int indexCount = 0;

    enum ModuleType {
        MAIN_CORRIDOR,
        HABITAT_MODULE,
        DOCKING_PORT,
        SOLAR_PANEL,
        ANTENNA
    } type = MAIN_CORRIDOR;
};

// L-System Rule structure
struct LSystemRule {
    char symbol;
    std::string replacement;
    float probability = 1.0f; // For stochastic L-systems
};

// L-System parameters for space station generation
struct LSystemParams {
    std::string axiom = "A"; // Starting symbol
    std::vector<LSystemRule> rules;
    int iterations = 3;
    float lengthScale = 0.7f; // How much to scale length each generation
    float radiusScale = 0.8f; // How much to scale radius each generation
    float branchAngle = 90.0f; // Angle for perpendicular branches
    float branchProbability = 1.0f; // Probability that a branch will produce children (0.0-1.0)
    unsigned int randomSeed = 0; // Seed for randomization
};

// Turtle state for L-system interpretation
struct TurtleState {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f); // Forward direction
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 left = glm::vec3(0.0f, 0.0f, 1.0f);
    float currentLength = 5.0f;
    float currentRadius = 1.0f;
    int generation = 0;
};

// Holds mesh and transform for each box (legacy support)
struct BoundCube {
    unsigned int vao = 0, vbo = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 color = glm::vec3(0.2, 0.7, 1.0); // default (unused by PBR)
};

// Create a cylinder mesh for a space station module
// Layout: location 0 = position (vec3), location 1 = normal (vec3), location 2 = uv (vec2)
void createCylinderMesh(StationModule& module, float length, float radius, int segments = 32);

// Generate a cuboid mesh (legacy support)
void createCuboidMesh(BoundCube& cube, float length, float width, float height);

// L-System string generation
std::string generateLSystemString(const LSystemParams& params);

// Interpret L-System string and generate space station modules
std::vector<StationModule> interpretLSystemToStation(
    const std::string& lSystemString,
    const LSystemParams& params);

// Create a complete procedural space station using L-systems
std::vector<StationModule> generateProceduralStation(
    const LSystemParams& params,
    float mainCylinderLength = 10.0f,
    float mainCylinderRadius = 1.5f);

// Create preset L-system parameters for different station types
LSystemParams createStandardStationParams();
LSystemParams createComplexStationParams();
LSystemParams createMinimalStationParams();

// Create custom L-system parameters - 5-argument version
LSystemParams createCustomStationParams(
    int iterations,
    float lengthScale,
    float radiusScale,
    float branchAngle,
    unsigned int randomSeed);

// Create custom L-system parameters - 6-argument version (for branch probability)
LSystemParams createCustomStationParams(
    int iterations,
    float lengthScale,
    float radiusScale,
    float branchAngle,
    float branchProbability,
    unsigned int randomSeed);

// Render station modules using PBR shader
void renderStationModulesPBR(
    const std::vector<StationModule>& modules,
    const glm::mat4& view,
    const glm::mat4& proj,
    unsigned int pbrShader);

// Legacy functions
std::vector<BoundCube> scatterBoundCubes(
    int count,
    float sphereRadius,
    float length, float width, float height);

void renderBoundCubesPBR(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj, unsigned int pbrShader);