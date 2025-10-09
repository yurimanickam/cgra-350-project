#include "station.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>
#include <random>
#include <ctime>

// Helper struct for vertex attributes
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Generates the vertex data for a cuboid procedurally
static void generateCuboidVertices(float length, float width, float height, std::vector<float>& outVertices) {
    // Each face: 4 vertices (quad), 2 triangles per face (6 indices)
    const glm::vec3 positions[8] = {
        {-0.5f, -0.5f, -0.5f}, // 0
        { 0.5f, -0.5f, -0.5f}, // 1
        { 0.5f,  0.5f, -0.5f}, // 2
        {-0.5f,  0.5f, -0.5f}, // 3
        {-0.5f, -0.5f,  0.5f}, // 4
        { 0.5f, -0.5f,  0.5f}, // 5
        { 0.5f,  0.5f,  0.5f}, // 6
        {-0.5f,  0.5f,  0.5f}, // 7
    };
    // Scale positions
    glm::vec3 scale = { length, height, width };
    std::array<glm::vec3, 8> scaled;
    for (int i = 0; i < 8; ++i) {
        scaled[i] = positions[i] * scale;
    }

    // Face definitions (indices into positions)
    struct Face {
        int idx[4];
        glm::vec3 normal;
    };
    constexpr Face faces[6] = {
        // Order: ccw for outward normal, +z=front
        {{0, 1, 2, 3}, { 0,  0, -1}}, // back
        {{4, 5, 6, 7}, { 0,  0,  1}}, // front
        {{0, 4, 7, 3}, {-1,  0,  0}}, // left
        {{1, 5, 6, 2}, { 1,  0,  0}}, // right
        {{0, 1, 5, 4}, { 0, -1,  0}}, // bottom
        {{3, 2, 6, 7}, { 0,  1,  0}}, // top
    };

    // UVs quad mapping
    const glm::vec2 uvs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    outVertices.clear();
    outVertices.reserve(6 * 6 * 8); // 6 faces, 2 triangles, 3 verts, 8 floats/vert

    // Each face: two triangles (0,1,2) and (2,3,0) in quad
    for (const Face& face : faces) {
        // Vertices of the quad
        Vertex quad[4];
        for (int i = 0; i < 4; ++i) {
            quad[i].pos = scaled[face.idx[i]];
            quad[i].normal = face.normal;
            quad[i].uv = uvs[i];
        }
        // Triangle 1: 0,1,2
        for (int i : {0, 1, 2}) {
            outVertices.push_back(quad[i].pos.x);
            outVertices.push_back(quad[i].pos.y);
            outVertices.push_back(quad[i].pos.z);
            outVertices.push_back(quad[i].normal.x);
            outVertices.push_back(quad[i].normal.y);
            outVertices.push_back(quad[i].normal.z);
            outVertices.push_back(quad[i].uv.x);
            outVertices.push_back(quad[i].uv.y);
        }
        // Triangle 2: 2,3,0
        for (int i : {2, 3, 0}) {
            outVertices.push_back(quad[i].pos.x);
            outVertices.push_back(quad[i].pos.y);
            outVertices.push_back(quad[i].pos.z);
            outVertices.push_back(quad[i].normal.x);
            outVertices.push_back(quad[i].normal.y);
            outVertices.push_back(quad[i].normal.z);
            outVertices.push_back(quad[i].uv.x);
            outVertices.push_back(quad[i].uv.y);
        }
    }
}

void createCuboidMesh(BoundCube& cube, float length, float width, float height) {
    std::vector<float> vertices;
    generateCuboidVertices(length, width, height, vertices);

    if (cube.vao != 0) {
        glDeleteVertexArrays(1, &cube.vao);
        glDeleteBuffers(1, &cube.vbo);
    }
    glGenVertexArrays(1, &cube.vao);
    glGenBuffers(1, &cube.vbo);

    glBindVertexArray(cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // pos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // uv (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

// ... scatterBoundCubes and renderBoundCubesPBR remain unchanged ...
std::vector<BoundCube> scatterBoundCubes(
    int count,
    const glm::vec3& bboxMin, const glm::vec3& bboxMax,
    float length, float width, float height)
{
    std::vector<BoundCube> cubes;
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_real_distribution<float> dx(bboxMin.x, bboxMax.x);
    std::uniform_real_distribution<float> dy(bboxMin.y, bboxMax.y);
    std::uniform_real_distribution<float> dz(bboxMin.z, bboxMax.z);
    std::uniform_real_distribution<float> rot(0, glm::two_pi<float>());

    for (int i = 0; i < count; ++i) {
        BoundCube cube;
        createCuboidMesh(cube, length, width, height);

        // Random position and orientation
        glm::vec3 pos(dx(rng), dy(rng), dz(rng));
        float angle = rot(rng);
        glm::vec3 axis = glm::normalize(glm::vec3(dx(rng), dy(rng), dz(rng)));
        if (glm::length(axis) < 0.01f) axis = glm::vec3(0, 1, 0);

        cube.model = glm::translate(glm::mat4(1.0f), pos)
            * glm::rotate(glm::mat4(1.0f), angle, axis);

        // Optional: random color (unused by PBR)
        cube.color = glm::vec3(0.2f + 0.6f * float(i) / count, 0.7f, 1.0f - 0.5f * float(i) / count);

        cubes.push_back(cube);
    }
    return cubes;
}

void renderBoundCubesPBR(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj, unsigned int pbrShader) {
    glUseProgram(pbrShader);

    // Set once (caller may already set these, but it's safe to set again)
    GLint locProj = glGetUniformLocation(pbrShader, "projection");
    GLint locView = glGetUniformLocation(pbrShader, "view");
    if (locProj != -1) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locView != -1) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

    // Draw each cube with its own model/normalMatrix
    for (const BoundCube& cube : cubes) {
        glm::mat4 model = cube.model;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

        GLint locModel = glGetUniformLocation(pbrShader, "model");
        GLint locNormal = glGetUniformLocation(pbrShader, "normalMatrix");
        if (locModel != -1) glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        if (locNormal != -1) glUniformMatrix3fv(locNormal, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        glBindVertexArray(cube.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glUseProgram(0);
}