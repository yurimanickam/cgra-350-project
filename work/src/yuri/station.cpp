#include "station.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <ctime>

// Shader for colored cubes (same as before)
namespace {
    GLuint cubeShader = 0;

    const char* cubeVert = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 view, proj, model;
        void main() {
            gl_Position = proj * view * model * vec4(aPos, 1.0);
        }
    )";
    const char* cubeFrag = R"(
        #version 330 core
        uniform vec3 uColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(uColor, 1.0);
        }
    )";

    GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    }

    void initCubeShader() {
        if (cubeShader) return;
        GLuint vs = compileShader(GL_VERTEX_SHADER, cubeVert);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, cubeFrag);
        cubeShader = glCreateProgram();
        glAttachShader(cubeShader, vs);
        glAttachShader(cubeShader, fs);
        glLinkProgram(cubeShader);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

void createCuboidMesh(BoundCube& cube, float length, float width, float height) {
    // Centered at origin. Vertex order: each face, 2 triangles per face.
    float x = length / 2.0f, y = height / 2.0f, z = width / 2.0f;
    float vertices[] = {
        // positions
        -x, -y, -z,   x, -y, -z,   x,  y, -z,   x,  y, -z,  -x,  y, -z,  -x, -y, -z, // back
        -x, -y,  z,   x, -y,  z,   x,  y,  z,   x,  y,  z,  -x,  y,  z,  -x, -y,  z, // front
        -x,  y,  z,  -x,  y, -z,  -x, -y, -z,  -x, -y, -z,  -x, -y,  z,  -x,  y,  z, // left
         x,  y,  z,   x,  y, -z,   x, -y, -z,   x, -y, -z,   x, -y,  z,   x,  y,  z, // right
        -x, -y, -z,   x, -y, -z,   x, -y,  z,   x, -y,  z,  -x, -y,  z,  -x, -y, -z, // bottom
        -x,  y, -z,   x,  y, -z,   x,  y,  z,   x,  y,  z,  -x,  y,  z,  -x,  y, -z  // top
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

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

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

        // Optional: random color
        cube.color = glm::vec3(0.2 + 0.6f * float(i) / count, 0.7, 1.0 - 0.5f * float(i) / count);

        cubes.push_back(cube);
    }
    return cubes;
}

void renderBoundCubes(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj) {
    initCubeShader();
    glUseProgram(cubeShader);
    for (const BoundCube& cube : cubes) {
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(cube.model));
        glUniform3fv(glGetUniformLocation(cubeShader, "uColor"), 1, glm::value_ptr(cube.color));
        glBindVertexArray(cube.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glUseProgram(0);
}