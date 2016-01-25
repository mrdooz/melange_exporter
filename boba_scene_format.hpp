#pragma once

// This is the actual binary format saved on disk
namespace protocol
{
#pragma pack(push, 1)

  enum
  {
    INVALID_OBJECT_ID = 0xffffffff
  };

  struct SceneBlob
  {
    char id[4];
    u32 version = 2;
    u32 flags;
    u32 fixupOffset;
    u32 nullObjectDataStart;
    u32 meshDataStart;
    u32 lightDataStart;
    u32 cameraDataStart;
    u32 materialDataStart;
    u32 numNullObjects;
    u32 numMeshes;
    u32 numLights;
    u32 numCameras;
    u32 numMaterials;
    // new in version 2
    u32 splineDataStart;
    u32 numSplines;
  };

  struct BlobBase
  {
    const char* name;
    u32 id;
    u32 parentId;
    float mtxLocal[12];
    float mtxGlobal[12];
  };

  struct MeshBlob : public BlobBase
  {
    struct MaterialGroup
    {
      u32 materialId;
      u32 startIndex;
      u32 numIndices;
    };

    u32 numVerts;
    u32 numIndices;
    u32 numMaterialGroups;
    MaterialGroup* materialGroups;
    float* verts;
    float* normals;
    float* uv;
    u32* indices;

    u32 numSelectedEdges;
    u32* selectedEdges;

    // bounding sphere
    float sx, sy, sz, r;
  };

  struct NullObjectBlob : public BlobBase
  {

  };

  struct CameraBlob : public BlobBase
  {
    float verticalFov;
    float nearPlane, farPlane;
    // new in version 3
    u32 targetId = INVALID_OBJECT_ID;
  };

  struct LightBlob : public BlobBase
  {
    int type;
    float colorRgb[3];
    float intensity;
  };

  struct SplineBlob : public BlobBase
  {
    int type;
    u32 numPoints;
    float* points;
    bool isCLosed;
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
#pragma pack(pop)

}
