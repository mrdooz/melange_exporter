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

namespace melange
{
  //-----------------------------------------------------------------------------
  class AlienMaterial : public Material
  {
    INSTANCEOF(AlienMaterial, Material)

  public:

    int id;

    AlienMaterial() : Material() { id = 0; }
    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  //self defined layer with own functions and members
  class AlienLayer : public LayerObject
  {
    INSTANCEOF(AlienLayer, LayerObject)

  public:

    int id;

    AlienLayer() : LayerObject() { }
    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  //self defined polygon object data with own functions and members
  class AlienPolygonObjectData : public PolygonObjectData
  {
    INSTANCEOF(AlienPolygonObjectData, PolygonObjectData)

  public:
    int layid;
    int matid;

    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  class AlienBaseDocument : public BaseDocument
  {
    INSTANCEOF(AlienBaseDocument, BaseDocument)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  // this self defined alien primitive object data will �understand� a Cube as parametric Cube
  // all other objects are unknown and we will get a polygonal description for them
  class AlienPrimitiveObjectData : public NodeData
  {
    INSTANCEOF(AlienPrimitiveObjectData, NodeData)
    int type_id;
  public:
    AlienPrimitiveObjectData(int id) : type_id(id) {}
    virtual Bool Execute();
  };
}

namespace boba
{
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

  struct Light
  {
    void Save(DeferredWriter& writer);
  };

  struct Camera
  {
    void Save(DeferredWriter& writer);
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
    void Save(DeferredWriter& writer);

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
    vector<MaterialGroup> materialGroups;
    unordered_map<u32, vector<int>> polysByMaterial;

    Sphere boundingSphere;
  };

  //------------------------------------------------------------------------------
  struct Options;
  struct Scene
  {
    bool Save(const Options& options);
    vector<Mesh> meshes;
    vector<Camera> cameras;
    vector<Light> lights;
    vector<Material> materials;
  };
}
