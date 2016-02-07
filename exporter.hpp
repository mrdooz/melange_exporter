#pragma once

using namespace std;

#define WITH_XFORM_MTX 0

namespace boba
{
  static u32 DEFAULT_MATERIAL = ~0u;

  struct Options
  {
    string inputFilename;
    string outputFilename;
    FILE* logfile = nullptr;
    bool compressVertices = false;
    bool compressIndices = false;
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
    Color(float r, float g, float b) : r(r), g(g), b(b) {}
    Color() : r(0), g(0), b(0) {}
    float r, g, b;
    // Note, alpha is written to disk, so don't remove this again :)
    float a = 1;
  };

  //------------------------------------------------------------------------------
  struct Sphere
  {
    Vec3<float> center;
    float radius;
  };

  //------------------------------------------------------------------------------
  struct Keyframe
  {
    int frame;
    float value;
  };

  //------------------------------------------------------------------------------
  struct Curve
  {
    string name;
    vector<Keyframe> keyframes;
  };

  //------------------------------------------------------------------------------
  struct Track
  {
    string name;
    vector<Curve> curves;
  };

  //------------------------------------------------------------------------------
  struct Transform
  {
    Vec3f pos;
    Vec3f rot;
    Vec3f scale;
  };

  //------------------------------------------------------------------------------
  struct BaseObject
  {
    BaseObject(melange::BaseObject* melangeObj);

    melange::BaseObject* melangeObj = nullptr;
    BaseObject* parent = nullptr;
#if WITH_XFORM_MTX
    float mtxLocal[12];
    float mtxGlobal[12];
#endif
    Transform xformLocal;
    Transform xformGlobal;
    string name;
    u32 id = ~0u;
    bool valid = true;

    vector<Track> animTracks;
  };

  //------------------------------------------------------------------------------
  struct Light : public BaseObject
  {
    Light(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}
    int type;
    Color color;
    float intensity;

    int falloffType;
    float falloffRadius;

    float outerAngle = 0.0f;
  };

  //------------------------------------------------------------------------------
  struct NullObject : public BaseObject
  {
    NullObject(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}
  };

  //------------------------------------------------------------------------------
  struct Camera : public BaseObject
  {
    Camera(melange::BaseObject* melangeObj) : BaseObject(melangeObj) {}

    BaseObject* targetObj = nullptr;
    float verticalFov;
    float nearPlane, farPlane;
  };

  //------------------------------------------------------------------------------
  struct MaterialComponent
  {
    Color color;
    string texture;
    float brightness;
  };

  //------------------------------------------------------------------------------
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
  struct Spline : public BaseObject
  {
    Spline(melange::SplineObject* melangeObj) : BaseObject(melangeObj) {}
    int type = 0;
    vector<float> points;
    bool isClosed = 0;
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
    vector<u32> selectedEdges;
    unordered_map<u32, vector<int>> polysByMaterial;

    Sphere boundingSphere;
  };

  //------------------------------------------------------------------------------
  struct Options;

  struct SceneStats
  {
    int nullObjectSize = 0;
    int cameraSize = 0;
    int meshSize = 0;
    int lightSize = 0;
    int materialSize = 0;
    int splineSize = 0;
    int animationSize = 0;
    int dataSize = 0;
  };

  struct Scene
  {
    boba::BaseObject* FindObject(melange::BaseObject* obj);
    vector<Mesh*> meshes;
    vector<Camera*> cameras;
    vector<NullObject*> nullObjects;
    vector<Light*> lights;
    vector<Material*> materials;
    vector<Spline*> splines;
    unordered_map<melange::BaseObject*, boba::BaseObject*> objMap;

    static u32 nextObjectId;
  };
}
