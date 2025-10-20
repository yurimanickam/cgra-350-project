#pragma once

// std
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

// glm
#include <glm/glm.hpp>

// project
#include "cgra/cgra_mesh.hpp"

namespace cgra {

    // Represents a part of a mesh that uses a single material
    struct mesh_group {
        std::string material_name;
        cgra::gl_mesh mesh;
    };

    // A model composed of multiple mesh groups
    struct multi_mesh_model {
        std::vector<mesh_group> mesh_groups;
        std::vector<std::string> material_names; // All unique materials found

        void destroy() {
            for (auto& group : mesh_groups) {
                group.mesh.destroy();
            }
        }
    };

    // Enhanced OBJ loader with better memory management and robustness
    class OBJLoader {
    private:
        // Internal structures for parsing
        struct ParsedVertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 texCoord;

            // For vertex deduplication
            bool operator==(const ParsedVertex& other) const {
                const float epsilon = 1e-6f;
                return glm::length(position - other.position) < epsilon &&
                    glm::length(normal - other.normal) < epsilon &&
                    glm::length(texCoord - other.texCoord) < epsilon;
            }
        };

        // Hash function for vertex deduplication
        struct ParsedVertexHash {
            std::size_t operator()(const ParsedVertex& v) const {
                auto h1 = std::hash<float>{}(v.position.x);
                auto h2 = std::hash<float>{}(v.position.y);
                auto h3 = std::hash<float>{}(v.position.z);
                auto h4 = std::hash<float>{}(v.normal.x);
                auto h5 = std::hash<float>{}(v.normal.y);
                auto h6 = std::hash<float>{}(v.normal.z);
                auto h7 = std::hash<float>{}(v.texCoord.x);
                auto h8 = std::hash<float>{}(v.texCoord.y);

                return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^
                    (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
            }
        };

        // Raw data from OBJ file
        std::vector<glm::vec3> m_positions;
        std::vector<glm::vec3> m_normals;
        std::vector<glm::vec2> m_texCoords;

        // Processed data for mesh building
        std::vector<mesh_vertex> m_vertices;
        std::unordered_map<ParsedVertex, unsigned int, ParsedVertexHash> m_vertexMap;

        // Grouping by material
        struct FaceGroup {
            std::string material_name;
            std::vector<unsigned int> indices;
        };
        std::vector<FaceGroup> m_faceGroups;
        std::string m_currentMaterial;
        std::vector<std::string> m_materialNames;
        std::string m_basePath;


        // Statistics
        size_t m_originalVertexCount;
        size_t m_optimizedVertexCount;

        // Helper functions
        void parseVertexPosition(const std::string& line);
        void parseVertexNormal(const std::string& line);
        void parseTextureCoord(const std::string& line);
        void parseFace(const std::string& line);
        void parseMtllib(const std::string& line);
        void parseUsemtl(const std::string& line);
        void processMtlFile(const std::string& mtl_filename);
        void generateNormalsIfMissing();
        void generateDefaultTexCoords();
        unsigned int addVertex(const ParsedVertex& vertex);
        void optimizeMesh();
        void printStatistics() const;

    public:
        OBJLoader();
        ~OBJLoader();

        // Main loading function
        bool loadFromFile(const std::string& filename);

        // Build the final multi-mesh model
        multi_mesh_model buildMultiMeshModel() const;

        // Get statistics
        size_t getTriangleCount() const;
        size_t getVertexCount() const { return m_vertices.size(); }
        size_t getOriginalVertexCount() const { return m_originalVertexCount; }

        // Clear all data
        void clear();
    };

    // Convenience function to load a multi-mesh model
    multi_mesh_model load_multi_mesh_model(const std::string& filename);
}