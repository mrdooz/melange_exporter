#pragma once
#include <stdint.h>

typedef uint32_t u32;

namespace boba
{
  struct BobaScene
  {
    char id[4];
    u32 fixupOffset;
    u32 meshDataStart;
    u32 lightDataStart;
    u32 cameraDataStart;
    u32 numMeshes;
    u32 numLights;
    u32 numCameras;
    char data[0];
  };

  struct MeshElement
  {
    const char* name;
    u32 numVerts;
    u32 numIndices;
    float* verts;
    float* normals;
    float* uv;
    u32* indices;
  };
}
