#include "station.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <ctime>

void createCuboidMesh(BoundCube& cube, float length, float width, float height) {
    // Centered at origin. Vertex order: each face, 2 triangles per face.
    // Attribute layout: position (3), normal (3), uv (2)
    float x = length / 2.0f, y = height / 2.0f, z = width / 2.0f;

    float vertices[] = {
        // back face (0,0,-1)
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         x, -y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -x,  y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,

        // front face (0,0,1)
        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         x, -y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -x,  y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

        // left face (-1,0,0)
        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -x,  y, -z, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y,  z, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

        // right face (1,0,0)
         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y, -z,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y,  z,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

         // bottom face (0,-1,0)
         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
          x, -y, -z,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         -x, -y,  z,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

         // top face (0,1,0)
         -x,  y, -z,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
          x,  y,  z,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
          x,  y, -z,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
          x,  y,  z,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         -x,  y, -z,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         -x,  y,  z,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };

    if (cube.vao != 0) {
        glDeleteVertexArrays(1, &cube.vao);
        glDeleteBuffers(1, &cube.vbo);
    }
    glGenVertexArrays(1, &cube.vao);
    glGenBuffers(1, &cube.vbo);

    glBindVertexArray(cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

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