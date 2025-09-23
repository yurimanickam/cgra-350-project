
#pragma once

// std
#include <string>
#include <vector>

// glm
#include <glm/glm.hpp>


enum dof {
	dof_none = 0,
	dof_rx   = 1 << 0,
	dof_ry   = 1 << 1,
	dof_rz   = 1 << 2,
	dof_root = 1 << 3
};
using dof_set = unsigned char;

// Type to represent a bone
struct skeleton_bone {
	std::string name;
	float length = 0;           // length of the bone
	glm::vec3 direction{0};     // direction of the bone
	glm::vec3 basis{0};         // angle-axis rotations for the bone basis
	dof_set freedom = dof_none; // degrees of freedom for the joint rotation
	std::vector<int> children;  // ids of the children of this bone
	glm::vec3 rotation_max{0};  // maximum value for rotation for this joint (degrees)
	glm::vec3 rotation_min{0};  // minimum value for rotation for this joint (degrees)
};

// structure for holding bone data
class skeleton_data {
public:
	std::vector<skeleton_bone> bones;

	// empty constructor as well
	skeleton_data() { }

	// load a skeleton from an ASF file
	skeleton_data(const std::string &file);

	// returns the id of a bone that matches the given name (-1 otherwise)
	int findBone(const std::string &name) const;

private:
	// helper functions for parsing
	void readASFHeader(const std::string &headerline, std::ifstream &file);
	void readASFBone(std::ifstream &file);
	void readHierarchy(std::ifstream &file);
};


// single pose for a bone of a skeleton
struct skeleton_bone_pose {
	glm::vec3 rotation{0};
	glm::vec3 translation{0};
};


// single pose for a skeleton, holds vector of transforms
struct skeleton_pose {
	std::vector<skeleton_bone_pose> boneTransforms;
};


// list of poses for a skeleton
struct skeleton_animation {
	std::vector<skeleton_pose> poses;

	skeleton_animation() { }

	// load an animation from an AMC file given associated skeleton data
	skeleton_animation(const std::string &file, const skeleton_data &skel);
};