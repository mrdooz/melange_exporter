#pragma once
#include <stdint.h>
#include <string>
#include <vector>

typedef uint32_t u32;
typedef uint8_t u8;

using namespace std;

namespace boba
{


  //------------------------------------------------------------------------------
  template <typename T>
  struct Vec3
  {
    Vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    Vec3() {}
    T x, y, z;
  };

  typedef Vec3<float> Vec3f;

  //------------------------------------------------------------------------------
  struct Sphere
  {
    Vec3<float> center;
    float radius;
  };

  //------------------------------------------------------------------------------
  struct Mesh
  {
    Mesh(u32 idx) : idx(idx) {}
    u32 idx;
    string name;
    vector<float> verts;
    vector<float> normals;
    vector<float> uv;
    vector<int> indices;

    Sphere boundingSphere;
  };

  //------------------------------------------------------------------------------
  struct Scene
  {
    bool Save(const char* filename);
    vector<Mesh*> meshes;
  };

  enum class SceneElement
  {
    Mesh,
    Camera,
    Light,
    NumElements
  };

  struct ElementChunk
  {
    u32 numElements;
#pragma warning(suppress: 4200)
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

  struct BobaScene
  {
    char id[4];
    u32 fixupOffset;
    u32 elementOffset[SceneElement::NumElements];
#pragma warning(suppress: 4200)
    char data[0];
  };
}
