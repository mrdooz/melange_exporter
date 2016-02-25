#pragma once

#ifndef BOBA_PROTOCOL_VERSION
#define BOBA_PROTOCOL_VERSION 5
#endif

// This is the actual binary format saved on disk
namespace protocol
{
#pragma pack(push, 1)

#define BIT(n) (1 << (n))

  // clang-format off
  enum VertexFormat2 : u16
  {
    VFORMAT2_POS = 0x1,
    VFORMAT2_POS_XY = 0x2,
    VFORMAT2_NORMAL = 0x3,
    VFORMAT2_TANGENT = 0x4,
    VFORMAT2_BINORMAL = 0x5,
    VFORMAT2_TEX2 = 0x6,
    VFORMAT2_TEX3 = 0x7,
    VFORMAT2_COLOR = 0x8,
    VFORMAT2_COLOR_U32 = 0x9,
  };

  enum VertexFlags2 : u16
  {
    VFLAG2_NONE = 0,
    VFLAG2_COMPRESSED = BIT(0),
  };
  // clang-format on

  enum
  {
    INVALID_OBJECT_ID = 0xffffffff
  };

  enum class LightType : u32
  {
    Point,
    Directional,
    Spot,
  };

  enum class FalloffType : u32
  {
    None = 0,
    Linear,
  };

  struct SceneBlob
  {
    char id[4];
    u32 version = 2;
    u32 flags;
    // offset of the deferred data
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
#if BOBA_PROTOCOL_VERSION >= 2
    u32 splineDataStart;
    u32 numSplines;
#endif
  };

  struct Transform
  {
    float pos[3];
    float rot[3];
    float scale[3];
  };

  struct BlobBase
  {
    const char* name;
    u32 id;
    u32 parentId;
#if BOBA_PROTOCOL_VERSION < 4
    float mtxLocal[12];
    float mtxGlobal[12];
#endif
#if BOBA_PROTOCOL_VERSION >= 4
    Transform xformLocal;
    Transform xformGlobal;
#endif
  };

  struct MeshBlob : public BlobBase
  {
    struct DataStream
    {
      const char* name;
      u32 flags;
      u32 numElems;
      u32 elemSize;
      void* data;
    };

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

    u32 numStreams;
    DataStream* streams;

#if BOBA_PROTOCOL_VERSION < 5
    u32* vertexFormat;
    float* vertexData;
    u32* indices;

    u32 numSelectedEdges;
    u32* selectedEdges;
#endif

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
#if BOBA_PROTOCOL_VERSION >= 3
    // new in version 3
    u32 targetId = INVALID_OBJECT_ID;
#endif
  };

  struct LightBlob : public BlobBase
  {
    LightType type;
    float color[4];
    float intensity;

    FalloffType falloffType;
    float falloffRadius;

    float outerAngle;
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
