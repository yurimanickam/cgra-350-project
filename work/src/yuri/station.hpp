#pragma once

#include "cgra/cgra_mesh.hpp"
#include <glm/glm.hpp>

class Station {
public:
    // Creates a cylinder mesh
    // radius - cylinder radius
    // height - cylinder height (centered at y=0, extends -height/2 to +height/2)
    // subdivisions - number of segments around the circumference
    // capped - whether to cap the cylinder at both ends
    cgra::gl_mesh createCylinderMesh(float radius, float height, int subdivisions, bool capped = true);


};