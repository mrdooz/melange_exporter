#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "boba_scene.hpp"


typedef uint32_t u32;
typedef uint8_t u8;

using namespace std;

namespace melange
{
  class BaseMaterial;
}

namespace boba
{

  class DeferredWriter;
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
  struct Color
  {
    Color(float r, float g, float b) : r(r), g(g), b(b), a(1) {}
    Color() : r(0), g(0), b(0), a(1) {}
    float r, g, b, a;
  };

  //------------------------------------------------------------------------------
  struct Sphere
  {
    Vec3<float> center;
    float radius;
  };

  //------------------------------------------------------------------------------

  struct MaterialComponent
  {
    Color color;
    string texture;
    float brightness;
  };

  struct Material
  {
    enum Flags
    { 
      FLAG_COLOR      = 1 << 0,
      FLAG_LUMINANCE  = 1 << 1,
      FLAG_REFLECTION = 1 << 2,
    };

    Material() : flags(0),  mat(nullptr), id(nextId++) {}

    string name;
    u32 flags;
    melange::BaseMaterial* mat;
    u32 id;

    MaterialComponent color;
    MaterialComponent luminance;
    MaterialComponent reflection;

    static u32 nextId;
  };

  //------------------------------------------------------------------------------
  struct Mesh
  {
    Mesh(u32 idx) : idx(idx) {}

    struct MaterialGroup
    {
      u32 materialId;
      vector<int> polys;
    };

    struct MaterialFaces
    {
      u32 materialId;
      u32 startTri = ~0u;
      u32 numTris = ~0u;
    };

    void Save(DeferredWriter& writer);
    u32 idx;
    string name;
    vector<float> verts;
    vector<float> normals;
    vector<float> uv;
    vector<int> indices;
    vector<MaterialFaces> materialFaces;
    unordered_map<u32, Mesh::MaterialGroup> materialGroups;

    Sphere boundingSphere;
  };

  //------------------------------------------------------------------------------
  struct Scene
  {
    bool Save(const char* filename);
    vector<Material> materials;
    vector<Mesh*> meshes;
  };
}
