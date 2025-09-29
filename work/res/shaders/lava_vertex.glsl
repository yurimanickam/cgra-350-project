#version 330 core

// Vertex attributes
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;

// Uniforms
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uModelMatrix;
uniform mat4 uNormalMatrix;

// New uniform to indicate fullscreen quad (pass-through)
uniform int uIsFullscreenQuad; // 0 = normal mesh, 1 = fullscreen quad

// Outputs to fragment shader
out vec3 FragPos;   // world position (used in glass/metal mode)
out vec3 Normal;    // world normal (used in glass/metal mode)
out vec2 TexCoord;  // UVs (used in metaball mode)
out vec3 ViewPos;   // view-space position (used in glass/metal mode)

void main() {
	// Pass UV through
	TexCoord = vUV;

	// Compute world position and normal (only meaningful for meshes)
	vec4 worldPos = uModelMatrix * vec4(vPosition, 1.0);
	FragPos = worldPos.xyz;
	Normal = normalize(mat3(uNormalMatrix) * vNormal);

	// View-space position (for lighting in mesh passes)
	vec4 viewPos = uModelViewMatrix * vec4(vPosition, 1.0);
	ViewPos = viewPos.xyz;

	// Final position in clip space
	// If rendering the fullscreen quad, vPosition is already NDC/clip coords
	if (uIsFullscreenQuad == 1) {
		// For safety, ensure a well-formed clip position: use vPosition.xy and keep z=0, w=1
		// (the createFullscreenQuad() sets positions in [-1,1] so this maps directly to clip)
		gl_Position = vec4(vPosition.xy, 0.0, 1.0);
	}
	else {
		gl_Position = uProjectionMatrix * viewPos;
	}
}