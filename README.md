# CGRA 350 Project - Team 11

**Team Members**
| Member | Contribution |
|-----------|-----------|
| David | Lava lamp simulation, raymarching and meatball rendering |
| Matt | PBR Rendering pipeline, Specular IBL lighting, Texture support. |
| Yuri | Procedural generation and space station geometry.|
---

## Project Overview

This project showcases an interactive 3D graphics scene that combines multiple advanced rendering techniques, including physically-based rendering (PBR), procedural generation, and real-time shader effects. The scene features a procedurally generated space station using L-systems, alongside a physically simulated lava lamp rendered using metaball and ray marching techniques. The lava lamp simulation incorporates spring-based physics and thermal dynamics for realistic motion and visual behavior.

---

## How to Build & Run

```bash
# Create a build directory
mkdir build

# Move into the build directory
cd build

# Generate build files using CMake
cmake ../work

# GLU.h might be needed on linux
sudo apt-get install libglu1-mesa-dev
sudo pacman -S glu

```
## UI Controls

To enable view of:  
Lava lamp - check `Show Lava Lamp` checkbox.  
PBR - check `Use Skybox` and or `Draw Sphere` checkbox.  
Space Station - press `Enable 3D View` button.  
