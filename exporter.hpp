#pragma once

#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>

typedef uint32_t u32;
typedef uint8_t u8;

using namespace std;

namespace boba
{
  struct Options
  {
    string inputFilename;
    string outputFilename;
    FILE* logfile = nullptr;
    bool shareVertices = true;
    int verbosity = 1;
  };

  class DeferredWriter;
  //------------------------------------------------------------------------------
  template <typename T>
  struct Vec3
  {
    template <typename U>
    Vec3(const U& v) : x(v.x), y(v.y), z(v.z) {}
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

  struct BaseObject
  {
    BaseObject(melange::BaseObject* melangeObj);
    BaseObject* parent = nullptr;
    melange::BaseObject* melangeObj = nullptr;
    float mtx[12];
    string name;
    u32 id = ~0u;
  };

  struct Light : public BaseObject
  {
  };

  struct NullObject : public BaseObject
  {
    NullObject(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}
  };

  struct Camera : public BaseObject
  {
    Camera(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}

    float verticalFov;
  };

  struct Material
  {
    enum Flags
    {
      FLAG_COLOR = 1 << 0,
      FLAG_LUMINANCE = 1 << 1,
      FLAG_REFLECTION = 1 << 2,
    };

    Material() : flags(0), mat(nullptr), id(nextId++) {}

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
  struct Mesh : public BaseObject
  {
    Mesh(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}

    struct MaterialGroup
    {
      u32 materialId;
      u32 startIndex = ~0u;
      u32 numIndices = ~0u;
    };

    vector<float> verts;
    vector<float> normals;
    vector<float> uv;
    vector<int> indices;
    vector<MaterialGroup> materialGroups;
    unordered_map<u32, vector<int>> polysByMaterial;

    Sphere boundingSphere;
  };

  //------------------------------------------------------------------------------
  struct Options;
  struct Scene
  {
    boba::BaseObject* FindObject(melange::BaseObject* obj);
    vector<Mesh*> meshes;
    vector<Camera*> cameras;
    vector<NullObject*> nullObjects;
    vector<Light*> lights;
    vector<Material*> materials;
    unordered_map<melange::BaseObject*, boba::BaseObject*> objMap;

    static u32 nextObjectId;
  };
}
