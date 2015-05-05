#pragma once
#include <stdint.h>

typedef uint32_t u32;

// This is the actual binary format saved on disk
namespace boba
{
  struct SceneBlob
  {
    char id[4];
    u32 fixupOffset;
    u32 meshDataStart;
    u32 lightDataStart;
    u32 cameraDataStart;
    u32 materialDataStart;
    u32 numMeshes;
    u32 numLights;
    u32 numCameras;
    u32 numMaterials;
#pragma warning(suppress: 4200)
    char data[0];
  };

  struct MeshBlob
  {
    struct MaterialGroup
    {
      u32 materialId;
      u32 startTri;
      u32 numTris;
    };

    const char* name;
    u32 numVerts;
    u32 numIndices;
    u32 numMaterialGroups;
    MaterialGroup* materialGroups;
    float* verts;
    float* normals;
    float* uv;
    u32* indices;

    float mtx[12];

    // bounding sphere
    float sx, sy, sz, r;
  };

  struct MaterialBlob
  {
    struct MaterialComponent
    {
      float r, g, b, a;
      const char* texture;
      float brightness;
    };

    u32 blobSize;
    const char* name;
    u32 materialId;
    u32 flags;
    MaterialComponent* color;
    MaterialComponent* luminance;
    MaterialComponent* reflection;
  };
}
