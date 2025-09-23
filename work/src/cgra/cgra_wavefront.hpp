
#pragma once

// std
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

// project
#include "cgra_mesh.hpp"


namespace cgra {

	inline mesh_builder load_wavefront_data(const std::string &filename) {
		using namespace std;
		using namespace glm;

		// struct for storing wavefront index data
		struct wavefront_vertex {
			unsigned int p, n, t;
		};

		// create reading buffers
		vector<vec3> positions;
		vector<vec3> normals;
		vector<vec2> uvs;
		vector<wavefront_vertex> wv_vertices;

		// open file
		ifstream objFile(filename);
		if (!objFile.is_open()) {
			cerr << "Error: could not open " << filename << endl;
			throw runtime_error("Error: could not open file " + filename);
		}

		// good() means that failbit, badbit and eofbit are all not set
		while (objFile.good()) {

			// Pull out line from file
			string line;
			getline(objFile, line);
			istringstream objLine(line);

			// Pull out mode from line
			string mode;
			objLine >> mode;

			// Reading like this means whitespace at the start of the line is fine
			// attempting to read from an empty string/line will set the failbit
			if (objLine.good()) {


				if (mode == "v") {
					vec3 v;
					objLine >> v.x >> v.y >> v.z;
					positions.push_back(v);
				}
				else if (mode == "vn") {
					vec3 vn;
					objLine >> vn.x >> vn.y >> vn.z;
					normals.push_back(vn);

				}
				else if (mode == "vt") {
					vec2 vt;
					objLine >> vt.x >> vt.y;
					uvs.push_back(vt);

				}
				else if (mode == "f") {

					std::vector<wavefront_vertex> face;
					while (objLine.good()) {
						wavefront_vertex v;

						// scan in position index
						objLine >> v.p;
						if (objLine.fail()) break;

						// look ahead for a match
						if (objLine.peek() == '/') {	
							// ignore the '/' character
							objLine.ignore(1);			

							// scan in uv (texture coord) index (if it's there)
							if (objLine.peek() != '/') {
								objLine >> v.t;
							}

							// scan in normal index (if it's there)
							if (objLine.peek() == '/') {
								objLine.ignore(1);
								objLine >> v.n;
							}
						}

						// subtract one because of wavefront indexing
						v.p -= 1;
						v.n -= 1;
						v.t -= 1;

						face.push_back(v);
					}

					// IFF we have 3 verticies, construct a triangle
					if (face.size() == 3) {
						for (int i = 0; i < 3; ++i) {
							wv_vertices.push_back(face[i]);
						}
					}
				}
			}
		}

		// if we don't have any normals, create them naively
		if (normals.empty()) {
			// Create the normals as 3d vectors of 0
			normals.resize(positions.size(), vec3(0));

			// add the normal for every face to each vertex-normal
			for (size_t i = 0; i < wv_vertices.size()/3; i++) {
				wavefront_vertex &a = wv_vertices[i*3];
				wavefront_vertex &b = wv_vertices[i*3+1];
				wavefront_vertex &c = wv_vertices[i*3+2];

				// set the normal index to be the same as position index
				a.n = a.p;
				b.n = b.p;
				c.n = c.p;

				// calculate the face normal
				vec3 ab = positions[b.p] - positions[a.p];
				vec3 ac = positions[c.p] - positions[a.p];
				vec3 face_norm = cross(ab, ac);

				// contribute the face norm to each vertex
				float l = length(face_norm);
				if (l > 0) {
					face_norm / l;
					normals[a.n] += face_norm;
					normals[b.n] += face_norm;
					normals[c.n] += face_norm;
				}
			}

			// normalize the normals
			for (size_t i = 0; i < normals.size(); i++) {
				normals[i] = normalize(normals[i]);
			}
		}

		// todo create spherical UV's if they don't exist

		// create mesh data
		mesh_builder mb;

		for (unsigned int i = 0; i < wv_vertices.size(); ++i) {
			mb.push_index(i);
			mb.push_vertex(mesh_vertex{
				positions[wv_vertices[i].p],
				normals[wv_vertices[i].n],
				uvs[wv_vertices[i].t]
			});
		}

		return mb;
	}
}