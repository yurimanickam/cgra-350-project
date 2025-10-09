#pragma once

#include <glm/glm.hpp>
#include <vector>

// Holds mesh and transform for each box
struct BoundCube {
    unsigned int vao = 0, vbo = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 color = glm::vec3(0.2, 0.7, 1.0); // default
    // Add more properties if needed (e.g., type, label)
};

// Generate a VAO+VBO mesh for a cuboid of given size (length, width, height)
void createCuboidMesh(BoundCube& cube, float length, float width, float height);

// Make a collection of cuboids randomly distributed in a bounding box
std::vector<BoundCube> scatterBoundCubes(
    int count,
    const glm::vec3& bboxMin, const glm::vec3& bboxMax,
    float length, float width, float height);

// Render all cuboids in the provided list with view/proj matrices
void renderBoundCubes(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj);
