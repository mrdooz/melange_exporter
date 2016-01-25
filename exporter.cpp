//-----------------------------------------------------------------------------
// Cinema4 melange exporter
// magnus.osterlind@gmail.com
//-----------------------------------------------------------------------------

#include "exporter.hpp"
#include "exporter_helpers.hpp"
#include "boba_scene_format.hpp"
#include "deferred_writer.hpp"

//-----------------------------------------------------------------------------
using namespace melange;
using namespace std;

namespace
{
  const float DEFAULT_NEAR_PLANE = 1.f;
  const float DEFAULT_FAR_PLANE = 1000.f;

  AlienBaseDocument* g_Doc;
  HyperFile* g_File;

  // Fixup functions called after the scene has been read and processed.
  vector<function<bool()>> g_deferredFunctions;
}

void CollectionAnimationTracks(BaseList2D* bl, vector<boba::Track>* tracks);

//------------------------------------------------------------------------------
static string ReplaceAll(const string& str, char toReplace, char replaceWith)
{
  string res(str);
  for (size_t i = 0; i < res.size(); ++i)
  {
    if (res[i] == toReplace)
      res[i] = replaceWith;
  }
  return res;
}

//------------------------------------------------------------------------------
string MakeCanonical(const string& str)
{
  // convert back slashes to forward
  return ReplaceAll(str, '\\', '/');
}

//-----------------------------------------------------------------------------

u32 boba::Scene::nextObjectId = 1;
extern bool SaveScene(
    const boba::Scene& scene, const boba::Options& options, boba::SceneStats* stats);

//-----------------------------------------------------------------------------
boba::Scene g_scene;
boba::Options options;

#define LOG(lvl, fmt, ...)                                                                         \
  if (options.verbosity >= lvl)                                                                    \
    printf(fmt, __VA_ARGS__);                                                                      \
  if (options.logfile)                                                                             \
    fprintf(options.logfile, fmt, __VA_ARGS__);

#define RANGE(c) (c).begin(), (c).end()

//-----------------------------------------------------------------------------
string CopyString(const melange::String& str)
{
  string res;
  if (char* c = str.GetCStringCopy())
  {
    res = string(c);
    DeleteMem(c);
  }
  return res;
}

//-----------------------------------------------------------------------------
void CopyMatrix(const Matrix& mtx, float* out)
{
  out[0] = (float)mtx.v1.x;
  out[1] = (float)mtx.v1.y;
  out[2] = (float)mtx.v1.z;
  out[3] = (float)mtx.v2.x;
  out[4] = (float)mtx.v2.y;
  out[5] = (float)mtx.v2.z;
  out[6] = (float)mtx.v3.x;
  out[7] = (float)mtx.v3.y;
  out[8] = (float)mtx.v3.z;
  out[9] = (float)mtx.off.x;
  out[10] = (float)mtx.off.y;
  out[11] = (float)mtx.off.z;
}

//-----------------------------------------------------------------------------
template <typename R, typename T>
R GetVectorParam(T* obj, int paramId)
{
  GeData data;
  obj->GetParameter(paramId, data);
  Vector v = data.GetVector();
  return R(v.x, v.y, v.z);
}

//-----------------------------------------------------------------------------
template <typename T>
float GetFloatParam(T* obj, int paramId)
{
  GeData data;
  obj->GetParameter(paramId, data);
  return (float)data.GetFloat();
}

//-----------------------------------------------------------------------------
template <typename T>
int GetInt32Param(T* obj, int paramId)
{
  GeData data;
  obj->GetParameter(paramId, data);
  return (float)data.GetInt32();
}

//-----------------------------------------------------------------------------
template <typename T>
inline bool IsQuad(const T& p)
{
  return p.c != p.d;
}

//-----------------------------------------------------------------------------
void AddIndices(vector<int>* indices, int a, int b, int c)
{
  indices->push_back(a);
  indices->push_back(b);
  indices->push_back(c);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddVector2(vector<float>* out, const T& v, bool flipY)
{
  out->push_back(v.x);
  out->push_back(flipY ? 1 - v.y : v.y);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddVector3(vector<float>* out, const T& v)
{
  out->push_back(v.x);
  out->push_back(v.y);
  out->push_back(v.z);
};

//-----------------------------------------------------------------------------
template <typename T>
void Add3Vector2(vector<float>* out, const T& a, const T& b, const T& c, bool flipY)
{
  AddVector2(out, a, flipY);
  AddVector2(out, b, flipY);
  AddVector2(out, c, flipY);
};

//-----------------------------------------------------------------------------
template <typename T>
void Add3Vector3(vector<float>* out, const T& a, const T& b, const T& c)
{
  AddVector3(out, a);
  AddVector3(out, b);
  AddVector3(out, c);
};

//-----------------------------------------------------------------------------
Vector CalcNormal(const Vector& a, const Vector& b, const Vector& c)
{
  Vector e0 = (b - a);
  e0.Normalize();

  Vector e1 = (c - a);
  e1.Normalize();

  return Cross(e0, e1);
}

//-----------------------------------------------------------------------------
void CollectVertices(PolygonObject* polyObj, boba::Mesh* mesh)
{
  // get point and polygon array pointer and counts
  const Vector* verts = polyObj->GetPointR();
  const CPolygon* polysOrg = polyObj->GetPolygonR();

  int vertexCount = polyObj->GetPointCount();
  if (!vertexCount)
    return;

  // calc bounding sphere (get center and max radius)
  Vector center(verts[0]);
  for (int i = 1; i < vertexCount; ++i)
  {
    center += verts[i];
  }
  center /= vertexCount;

  float radius = (center - verts[1]).GetSquaredLength();
  for (int i = 2; i < vertexCount; ++i)
  {
    float cand = (center - verts[i]).GetSquaredLength();
    radius = max(radius, cand);
  }

  mesh->boundingSphere.center = center;
  mesh->boundingSphere.radius = sqrtf(radius);

  // Loop over all the materials, and add the polygons in the order they appear per material
  struct Polygon
  {
    int a, b, c, d;
  };

  vector<Polygon> polys;
  polys.reserve(polyObj->GetPolygonCount());

  int numPolys = 0;
  int idx = 0;
  for (const pair<u32, vector<int>>& kv : mesh->polysByMaterial)
  {
    boba::Mesh::MaterialGroup mg;
    mg.materialId = kv.first;
    mg.startIndex = idx;
    numPolys += (int)kv.second.size();

    for (int i : kv.second)
    {
      polys.push_back({polysOrg[i].a, polysOrg[i].b, polysOrg[i].c, polysOrg[i].d});

      // increment index counter. if the polygon is a quad, then double the increment
      if (polysOrg[i].c == polysOrg[i].d)
        idx += 3;
      else
        idx += 2 * 3;
    }
    mg.numIndices = idx - mg.startIndex;
    mesh->materialGroups.push_back(mg);
  }

  u32 numVerts = 0;
  for (int i = 0; i < numPolys; ++i)
  {
    numVerts += IsQuad(polys[i]) ? 4 : 3;
  }

  // Check what kind of normals exist
  bool hasNormalsTag = !!polyObj->GetTag(Tnormal);
  bool hasPhongTag = !!polyObj->GetTag(Tphong);
  bool hasNormals = hasNormalsTag || hasPhongTag;

  Vector32* phongNormals = hasPhongTag ? polyObj->CreatePhongNormals() : nullptr;
  NormalTag* normals = hasNormalsTag ? (NormalTag*)polyObj->GetTag(Tnormal) : nullptr;
  UVWTag* uvs = (UVWTag*)polyObj->GetTag(Tuvw);
  ConstUVWHandle uvHandle = uvs ? uvs->GetDataAddressR() : nullptr;

  // add verts
  mesh->verts.reserve(numVerts * 3);
  mesh->normals.reserve(numVerts * 3);
  mesh->indices.reserve(numVerts * 3);
  mesh->uv.reserve(numVerts * 3);

  ConstNormalHandle normalHandle = normals ? normals->GetDataAddressR() : nullptr;

  u32 vertOfs = 0;

  for (int i = 0; i < numPolys; ++i)
  {
    int idx0 = polys[i].a;
    int idx1 = polys[i].b;
    int idx2 = polys[i].c;
    int idx3 = polys[i].d;
    bool isQuad = IsQuad(polys[i]);

    // face 0, 1, 2
    Add3Vector3(&mesh->verts, verts[idx0], verts[idx1], verts[idx2]);
    AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 1, vertOfs + 2);

    if (isQuad)
    {
      // face 0, 2, 3
      AddVector3(&mesh->verts, verts[idx3]);
      AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 2, vertOfs + 3);
      vertOfs += 4;
    }
    else
    {
      vertOfs += 3;
    }

    if (hasNormals)
    {
      if (hasNormalsTag)
      {
        NormalStruct normal;
        normals->Get(normalHandle, i, normal);
        Add3Vector3(&mesh->normals, normal.a, normal.b, normal.c);

        if (isQuad)
        {
          AddVector3(&mesh->normals, normal.d);
        }
      }
      else if (hasPhongTag)
      {
        Add3Vector3(&mesh->normals,
            phongNormals[i * 4 + 0],
            phongNormals[i * 4 + 1],
            phongNormals[i * 4 + 2]);

        if (isQuad)
        {
          AddVector3(&mesh->normals, phongNormals[i * 4 + 3]);
        }
      }
    }
    else
    {
      // no normals, so generate polygon normals and use them for all verts
      Vector normal = CalcNormal(verts[idx0], verts[idx1], verts[idx2]);
      Add3Vector3(&mesh->normals, normal, normal, normal);
      if (isQuad)
      {
        AddVector3(&mesh->normals, normal);
      }
    }

    if (uvHandle)
    {
      UVWStruct s;
      UVWTag::Get(uvHandle, i, s);

      Add3Vector2(&mesh->uv, s.a, s.b, s.c, true);
      if (isQuad)
      {
        // face 0, 2, 3
        AddVector2(&mesh->uv, s.d, true);
      }
    }
  }

  if (phongNormals)
    DeleteMem(phongNormals);
}

//-----------------------------------------------------------------------------
void CollectMaterials(AlienBaseDocument* c4dDoc)
{
  // add default material
  g_scene.materials.push_back(new boba::Material());
  boba::Material* exporterMaterial = g_scene.materials.back();
  exporterMaterial->mat = nullptr;
  exporterMaterial->name = "<default>";
  exporterMaterial->id = ~0u;
  exporterMaterial->flags = boba::Material::FLAG_COLOR;
  exporterMaterial->color.brightness = 1.f;
  exporterMaterial->color.color = boba::Color(0.5f, 0.5f, 0.5f);

  for (BaseMaterial* mat = c4dDoc->GetFirstMaterial(); mat; mat = mat->GetNext())
  {
    // check if the material is a standard material
    if (mat->GetType() != Mmaterial)
      continue;

    string name = CopyString(mat->GetName());

    g_scene.materials.push_back(new boba::Material());
    boba::Material* exporterMaterial = g_scene.materials.back();
    exporterMaterial->mat = mat;
    exporterMaterial->name = name;

    // check if the given channel is used in the material
    if (((melange::Material*)mat)->GetChannelState(CHANNEL_COLOR))
    {
      exporterMaterial->flags |= boba::Material::FLAG_COLOR;
      exporterMaterial->color.brightness = GetFloatParam(mat, MATERIAL_COLOR_BRIGHTNESS);
      exporterMaterial->color.color = GetVectorParam<boba::Color>(mat, MATERIAL_COLOR_COLOR);
    }

    if (((melange::Material*)mat)->GetChannelState(CHANNEL_REFLECTION))
    {
      exporterMaterial->flags |= boba::Material::FLAG_REFLECTION;
      exporterMaterial->reflection.brightness = GetFloatParam(mat, MATERIAL_REFLECTION_BRIGHTNESS);
      exporterMaterial->reflection.color =
          GetVectorParam<boba::Color>(mat, MATERIAL_REFLECTION_COLOR);
    }
  }
}

//-----------------------------------------------------------------------------
void CollectMeshMaterials(PolygonObject* obj, boba::Mesh* mesh)
{
  unordered_set<u32> selectedPolys;

  u32 prevMaterial = ~0u;
  u32 firstMaterial = ~0u;

  // For each material found, check if the following tag is a selection tag, in which
  // case record which polys belong to it
  for (BaseTag* btag = obj->GetFirstTag(); btag; btag = btag->GetNext())
  {
    // texture tag
    if (btag->GetType() == Ttexture)
    {
      GeData data;
      AlienMaterial* mat =
          btag->GetParameter(TEXTURETAG_MATERIAL, data) ? (AlienMaterial*)data.GetLink() : NULL;
      if (!mat)
        continue;

      auto materialIt = find_if(RANGE(g_scene.materials),
          [mat](const boba::Material* m)
          {
            return m->mat == mat;
          });
      if (materialIt == g_scene.materials.end())
        continue;

      boba::Material* bobaMaterial = *materialIt;
      prevMaterial = bobaMaterial->id;
      if (firstMaterial == ~0u)
        firstMaterial = bobaMaterial->id;
    }

    // Polygon Selection Tag
    if (btag->GetType() == Tpolygonselection && obj->GetType() == Opolygon)
    {
      // skip this selection tag if we can't find a previous material. i don't like relying things
      // just appearing in the correct order, but right now i don't know of a better way
      if (prevMaterial == ~0u)
      {
        continue;
      }

      if (BaseSelect* bs = ((SelectionTag*)btag)->GetBaseSelect())
      {
        auto& polysByMaterial = mesh->polysByMaterial[prevMaterial];
        for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
        {
          if (bs->IsSelected(i))
          {
            polysByMaterial.push_back(i);
            selectedPolys.insert(i);
          }
        }
      }

      // reset the previous material flag to avoid double selection tags causing trouble
      prevMaterial = ~0u;
    }
  }

  // if no materials are found, just add them to a dummy material
  if (firstMaterial == ~0u)
  {
    u32 dummyMaterial = boba::DEFAULT_MATERIAL;
    auto& polysByMaterial = mesh->polysByMaterial[dummyMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      polysByMaterial.push_back(i);
    }
  }
  else
  {
    // add all the polygons that aren't selected to the first material
    auto& polysByMaterial = mesh->polysByMaterial[firstMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      if (selectedPolys.count(i) == 0)
        polysByMaterial.push_back(i);
    }
  }

  for (auto g : mesh->polysByMaterial)
  {
    const char* materialName =
        g.first == boba::DEFAULT_MATERIAL ? "<default>" : g_scene.materials[g.first]->name.c_str();
    LOG(2, "material: %s, %d polys\n", materialName, (int)g.second.size());
  }
}

//-----------------------------------------------------------------------------
boba::BaseObject::BaseObject(melange::BaseObject* melangeObj)
    : melangeObj(melangeObj)
    , parent(g_scene.FindObject(melangeObj->GetUp()))
    , name(CopyString(melangeObj->GetName()))
    , id(Scene::nextObjectId++)
{
  LOG(1, "Exporting: %s\n", name.c_str());
  melange::BaseObject* melangeParent = melangeObj->GetUp();
  if ((!!melangeParent) ^ (!!parent))
  {
    LOG(1,
        "  Unable to find parent! (%s)\n",
        melangeParent ? CopyString(melangeParent->GetName()).c_str() : "");
  }

  g_scene.objMap[melangeObj] = this;
}

//-----------------------------------------------------------------------------
Bool AlienPrimitiveObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();

  string name(CopyString(baseObj->GetName()));
  LOG(1, "Skipping primitive object: %s\n", name.c_str());
  return true;
}

//-----------------------------------------------------------------------------
void CollectionAnimationTracks(BaseList2D* bl, vector<boba::Track>* tracks)
{
  if (!bl || !bl->GetFirstCTrack())
    return;

  for (CTrack* ct = bl->GetFirstCTrack(); ct; ct = ct->GetNext())
  {
    // CTrack name
    boba::Track track;
    track.name = CopyString(ct->GetName());

    // time track
    CTrack* tt = ct->GetTimeTrack(bl->GetDocument());
    if (tt)
    {
      LOG(1, "Time track is unsupported");
    }

    // get DescLevel id
    DescID testID = ct->GetDescriptionID();
    DescLevel lv = testID[0];
    ct->SetDescriptionID(ct, testID);

    // get CCurve and print key frame data
    CCurve* cc = ct->GetCurve();
    if (cc)
    {
      boba::Curve curve;
      curve.name = CopyString(cc->GetName());

      for (Int32 k = 0; k < cc->GetKeyCount(); k++)
      {
        CKey* ck = cc->GetKey(k);
        BaseTime t = ck->GetTime();
        if (ct->GetTrackCategory() == PSEUDO_VALUE)
        {
          curve.keyframes.push_back(boba::Keyframe{(int)t.GetFrame(g_Doc->GetFps()), ck->GetValue()});
        }
        else if (ct->GetTrackCategory() == PSEUDO_PLUGIN && ct->GetType() == CTpla)
        {
          LOG(1, "Plugin keyframes are unsupported");
        }
        else if (ct->GetTrackCategory() == PSEUDO_PLUGIN && ct->GetType() == CTmorph)
        {
          LOG(1, "Morph keyframes are unsupported");
        }
      }

      track.curves.push_back(curve);
    }
    tracks->push_back(track);
  }
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  PolygonObject* polyObj = (PolygonObject*)baseObj;

  boba::Mesh* mesh = new boba::Mesh(baseObj);
  CollectionAnimationTracks(baseObj, &mesh->animTracks);

  CollectMeshMaterials(polyObj, mesh);
  CollectVertices(polyObj, mesh);

  CopyMatrix(polyObj->GetMl(), mesh->mtxLocal);
  CopyMatrix(polyObj->GetMg(), mesh->mtxGlobal);
  g_scene.meshes.push_back(mesh);

  return true;
}

//-----------------------------------------------------------------------------
Bool AlienCameraObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  const string name = CopyString(baseObj->GetName());

  // Check if the camera is a target camera
  BaseTag* targetTag = baseObj->GetTag(Ttargetexpression);
  if (!targetTag)
  {
    // Not a target camera, so require the parten object to be a null object
    BaseObject* parent = baseObj->GetUp();
    bool isNullParent = parent && parent->GetType() == OBJECT_NULL;
    if (!isNullParent)
    {
      LOG(1, "Camera's %s parent isn't a null object!\n", name.c_str());
      return false;
    }
  }

  int projectionType = GetInt32Param(baseObj, CAMERA_PROJECTION);
  if (projectionType != Pperspective)
  {
    LOG(2,
        "Skipping camera (%s) with unsupported projection type (%d)\n",
        name.c_str(),
        projectionType);
    return false;
  }

  boba::Camera* camera = new boba::Camera(baseObj);

  if (targetTag)
  {
    BaseObject* targetObj = targetTag->GetDataInstance()->GetObjectLink(TARGETEXPRESSIONTAG_LINK);
    g_deferredFunctions.push_back([=]() {
      camera->targetObj = g_scene.FindObject(targetObj);
      if (!camera->targetObj)
      {
        LOG(1, "Unable to find target object: %s", CopyString(targetObj->GetName()).c_str());
        return false;
      }
      return true;
    });
  }

  CollectionAnimationTracks(baseObj, &camera->animTracks);
  CopyMatrix(baseObj->GetMl(), camera->mtxLocal);
  CopyMatrix(baseObj->GetMg(), camera->mtxGlobal);

  camera->verticalFov = GetFloatParam(baseObj, CAMERAOBJECT_FOV_VERTICAL);
  camera->nearPlane =
      GetInt32Param(baseObj, CAMERAOBJECT_NEAR_CLIPPING_ENABLE)
          ? max(DEFAULT_NEAR_PLANE, GetFloatParam(baseObj, CAMERAOBJECT_NEAR_CLIPPING))
          : DEFAULT_NEAR_PLANE;
  camera->farPlane = GetInt32Param(baseObj, CAMERAOBJECT_FAR_CLIPPING_ENABLE)
                         ? GetFloatParam(baseObj, CAMERAOBJECT_FAR_CLIPPING)
                         : DEFAULT_FAR_PLANE;

  g_scene.cameras.push_back(camera);
  return true;
}

//-----------------------------------------------------------------------------
void GetChildren(BaseObject* obj, vector<BaseObject*>* children)
{
  BaseObject* child = obj->GetDown();
  while (child)
  {
    children->push_back(child);
    child = child->GetNext();
  }
}

//-----------------------------------------------------------------------------
void ExportSpline(BaseObject* obj)
{
  SplineObject* splineObject = static_cast<SplineObject*>(obj);
  string splineName = CopyString(splineObject->GetName());

  SPLINETYPE splineType = splineObject->GetSplineType();
  bool isClosed = splineObject->GetIsClosed();
  int pointCount = splineObject->GetPointCount();
  const Vector* points = splineObject->GetPointR();

  boba::Spline* s = new boba::Spline(splineObject);
  s->type = splineType;
  s->isClosed = isClosed;

  s->points.reserve(pointCount * 3);
  for (int i = 0; i < pointCount; ++i)
  {
    s->points.push_back(points[i].x);
    s->points.push_back(points[i].y);
    s->points.push_back(points[i].z);
  }

  g_scene.splines.push_back(s);
}

//-----------------------------------------------------------------------------
bool AlienNullObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();

  const string name = CopyString(baseObj->GetName());

  boba::NullObject* nullObject = new boba::NullObject(baseObj);
  CollectionAnimationTracks(baseObj, &nullObject->animTracks);
  CopyMatrix(baseObj->GetMl(), nullObject->mtxLocal);
  CopyMatrix(baseObj->GetMg(), nullObject->mtxGlobal);

  g_scene.nullObjects.push_back(nullObject);

  // Export spline objects that are children to the null object
  vector<BaseObject*> children;
  GetChildren(baseObj, &children);
  for (BaseObject* obj : children)
  {
    Int32 type = obj->GetType();
    switch (type)
    {
      case Ospline:
      {
        ExportSpline(obj);
        break;
      }
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
bool AlienLightObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  const string name = CopyString(baseObj->GetName());

  int lightType = GetInt32Param(baseObj, LIGHT_TYPE);
  if (lightType != LIGHT_TYPE_OMNI)
  {
    LOG(1, "Only omni lights are supported: %s\n", name.c_str());
    return true;
  }

  boba::Light* light = new boba::Light(baseObj);
  CollectionAnimationTracks(baseObj, &light->animTracks);
  CopyMatrix(baseObj->GetMl(), light->mtxLocal);
  CopyMatrix(baseObj->GetMg(), light->mtxGlobal);

  light->type = GetInt32Param(baseObj, LIGHT_TYPE);
  light->color = GetVectorParam<boba::Color>(baseObj, LIGHT_COLOR);
  light->intensity = GetFloatParam(baseObj, LIGHT_BRIGHTNESS);

  g_scene.lights.push_back(light);

  return true;
}

//-----------------------------------------------------------------------------
string FilenameFromInput(const string& inputFilename, bool stripPath)
{
  const char* ff = inputFilename.c_str();
  const char* dot = strrchr(ff, '.');
  if (dot)
  {
    int lenToDot = dot - ff;
    int startPos = 0;

    if (stripPath)
    {
      const char* slash = strrchr(ff, '/');
      startPos = slash ? slash - ff + 1 : 0;
    }
    return inputFilename.substr(startPos, lenToDot - startPos) + ".boba";
  }

  printf("Invalid input filename given: %s\n", inputFilename.c_str());
  return string();
}

//-----------------------------------------------------------------------------
int ParseOptions(int argc, char** argv)
{
  // format is [--verbose N] input [output]

  if (argc < 2)
  {
    printf("No input file specified\n");
    return 1;
  }

  int curArg = 1;
  int remaining = argc - 1;

  const auto& step = [&curArg, &remaining](int steps)
  {
    curArg += steps;
    remaining -= steps;
  };

  while (remaining)
  {
    if (strcmp(argv[curArg], "--compress-vertices") == 0)
    {
      step(1);
    }
    else if (strcmp(argv[curArg], "--compress-indices") == 0)
    {
      step(1);
    }
    else if (strcmp(argv[curArg], "--verbose") == 0)
    {
      // we need at least 1 more argument (for the level)
      if (remaining < 2)
      {
        printf("Invalid args\n");
        return 1;
      }
      options.verbosity = atoi(argv[curArg + 1]);
      step(2);
    }
    else
    {
      // Unknown arg, so break, and use this as the filename
      break;
    }
  }

  if (remaining < 1)
  {
    printf("Invalid args\n");
    return 1;
  }

  options.inputFilename = MakeCanonical(argv[curArg]);
  step(1);

  // create output file
  if (remaining == 1)
  {
    // check if the remaining argument is a file name, or just a directory
    if (strstr(argv[curArg], "boba") != nullptr)
    {
      options.outputFilename = argv[curArg];
    }
    else
    {
      // a directory was given, so create the filename from the input file
      string res = FilenameFromInput(options.inputFilename, true);
      if (res.empty())
        return 1;

      options.outputFilename = string(argv[curArg]) + '/' + res;
    }
  }
  else
  {
    options.outputFilename = FilenameFromInput(options.inputFilename, false);
    if (options.outputFilename.empty())
      return 1;
  }

  options.logfile = fopen((options.outputFilename + ".log").c_str(), "at");

  return 0;
}

u32 boba::Material::nextId;

//------------------------------------------------------------------------------
#ifndef _WIN32
template <typename T, typename U>
constexpr size_t offsetOf(U T::*member)
{
  return (char*)&((T*)nullptr->*member) - (char*)nullptr;
}
#endif

//------------------------------------------------------------------------------
boba::BaseObject* boba::Scene::FindObject(melange::BaseObject* obj)
{
  auto it = objMap.find(obj);
  return it == objMap.end() ? nullptr : it->second;
}

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  int optRes = ParseOptions(argc, argv);
  if (optRes)
    return optRes;

  time_t startTime = time(0);
  struct tm* now = localtime(&startTime);

  LOG(1,
      "==] STARTING [=================================] %.4d:%.2d:%.2d-%.2d:%.2d:%.2d ]==\n%s -> "
      "%s\n",
      now->tm_year + 1900,
      now->tm_mon + 1,
      now->tm_mday,
      now->tm_hour,
      now->tm_min,
      now->tm_sec,
      options.inputFilename.c_str(),
      options.outputFilename.c_str());

  g_Doc = NewObj(AlienBaseDocument);
  g_File = NewObj(HyperFile);

  if (!g_File->Open(DOC_IDENT, options.inputFilename.c_str(), FILEOPEN_READ))
    return 1;

  if (!g_Doc->ReadObject(g_File, true))
    return 1;

  g_File->Close();

  CollectMaterials(g_Doc);
  g_Doc->CreateSceneFromC4D();
  bool res = true;
  for (auto& fn : g_deferredFunctions)
  {
    res &= fn();
    if (!res)
      break;
  }

  int fps = g_Doc->GetFps();

  boba::SceneStats stats;
  if (res)
  {
    SaveScene(g_scene, options, &stats);
  }

  DeleteObj(g_Doc);
  DeleteObj(g_File);

  time_t endTime = time(0);
  now = localtime(&endTime);

  LOG(1,
      "--> stats: \n"
      "    null object size: %.2f kb\n"
      "    camera object size: %.2f kb\n"
      "    mesh object size: %.2f kb\n"
      "    light object size: %.2f kb\n"
      "    material object size: %.2f kb\n"
      "    spline object size: %.2f kb\n"
      "    animation object size: %.2f kb\n"
      "    data object size: %.2f kb\n",
      (float)stats.nullObjectSize / 1024,
      (float)stats.cameraSize / 1024,
      (float)stats.meshSize / 1024,
      (float)stats.lightSize / 1024,
      (float)stats.materialSize / 1024,
      (float)stats.splineSize / 1024,
      (float)stats.animationSize / 1024,
      (float)stats.dataSize / 1024);

  LOG(1,
      "==] DONE [=====================================] %.4d:%.2d:%.2d-%.2d:%.2d:%.2d ]==\n",
      now->tm_year + 1900,
      now->tm_mon + 1,
      now->tm_mday,
      now->tm_hour,
      now->tm_min,
      now->tm_sec);

  if (options.logfile)
    fclose(options.logfile);

  return 0;
}
