#pragma once

// glm
#include <glm/glm.hpp>

// project
#include "opengl.hpp"
#include "skeleton.hpp"

class skeleton_model {
private:
	// recursive helper method
	void drawBone(const glm::mat4 &view, int boneid);

public:
	GLuint shader = 0;
	skeleton_data skel;
	skeleton_pose pose;

	skeleton_model() { }
	void draw(const glm::mat4 &view, const glm::mat4 &proj);
};