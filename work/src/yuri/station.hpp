#include "station.hpp"
#include <vector>
#include <cmath>
#include <cgra/cgra_mesh.hpp>
#include <glm/gtc/constants.hpp>

cgra::gl_mesh Station::createCylinderMesh(float radius, float height, int subdivisions, bool capped) {
    using namespace glm;
    using namespace cgra;

    mesh_builder builder(GL_TRIANGLES);

    float halfHeight = height / 2.0f;
    float deltaTheta = 2.0f * glm::pi<float>() / float(subdivisions);

    // Vertices for the sides
    for (int i = 0; i <= subdivisions; ++i) {
        float theta = i * deltaTheta;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);

        vec3 normal = glm::normalize(vec3(x, 0, z));
        float u = float(i) / subdivisions;

        // Bottom vertex
        builder.vertices.push_back({ vec3(x, -halfHeight, z), normal, vec2(u, 0) });
        // Top vertex
        builder.vertices.push_back({ vec3(x, +halfHeight, z), normal, vec2(u, 1) });
    }

    // Indices for the sides
    for (int i = 0; i < subdivisions; ++i) {
        int idx = i * 2;
        // Each quad: 2 triangles
        builder.indices.push_back(idx);
        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 2);

        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 3);
        builder.indices.push_back(idx + 2);
    }

    // Caps
    if (capped) {
        int baseIndex = int(builder.vertices.size());

        // Bottom center
        builder.vertices.push_back({ vec3(0, -halfHeight, 0), vec3(0, -1, 0), vec2(0.5f, 0.5f) });
        int bottomCenterIdx = baseIndex;

        // Bottom rim
        for (int i = 0; i <= subdivisions; ++i) {
            float theta = i * deltaTheta;
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
            builder.vertices.push_back({ vec3(x, -halfHeight, z), vec3(0, -1, 0), uv });
        }
        for (int i = 0; i < subdivisions; ++i) {
            builder.indices.push_back(bottomCenterIdx);
            builder.indices.push_back(bottomCenterIdx + i + 1);
            builder.indices.push_back(bottomCenterIdx + i + 2);
        }

        baseIndex = int(builder.vertices.size());
        // Top center
        builder.vertices.push_back({ vec3(0, +halfHeight, 0), vec3(0, 1, 0), vec2(0.5f, 0.5f) });
        int topCenterIdx = baseIndex;

        // Top rim
        for (int i = 0; i <= subdivisions; ++i) {
            float theta = i * deltaTheta;
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
            builder.vertices.push_back({ vec3(x, +halfHeight, z), vec3(0, 1, 0), uv });
        }
        for (int i = 0; i < subdivisions; ++i) {
            builder.indices.push_back(topCenterIdx);
            builder.indices.push_back(topCenterIdx + i + 2);
            builder.indices.push_back(topCenterIdx + i + 1);
        }
    }

    return builder.build();
}