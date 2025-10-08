#include "station.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Temporary: simple cube shader and VAO storage
namespace {
    GLuint tempCubeVAO = 0;
    GLuint tempCubeVBO = 0;
    GLuint tempCubeShader = 0;

    // Cube vertex data: positions only
    float tempCubeVertices[] = {
        // positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    // Minimal vertex + fragment shader for colored cube
    const char* tempCubeVert = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 view;
        uniform mat4 proj;
        uniform mat4 model;
        void main() {
            gl_Position = proj * view * model * vec4(aPos, 1.0);
        }
    )";
    const char* tempCubeFrag = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.2, 0.7, 1.0, 1.0); // Cyan-ish
        }
    )";

    GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    }

    void initTempCubeGL() {
        if (tempCubeVAO)
            return;

        // VAO + VBO
        glGenVertexArrays(1, &tempCubeVAO);
        glGenBuffers(1, &tempCubeVBO);
        glBindVertexArray(tempCubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, tempCubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(tempCubeVertices), tempCubeVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // Shader program
        GLuint vs = compileShader(GL_VERTEX_SHADER, tempCubeVert);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, tempCubeFrag);
        tempCubeShader = glCreateProgram();
        glAttachShader(tempCubeShader, vs);
        glAttachShader(tempCubeShader, fs);
        glLinkProgram(tempCubeShader);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

void renderTempCube(const glm::mat4& view, const glm::mat4& proj) {
    initTempCubeGL();
    glUseProgram(tempCubeShader);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(tempCubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(tempCubeShader, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(tempCubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(tempCubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glUseProgram(0);
}