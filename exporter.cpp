//-----------------------------------------------------------------------------
// Cinema4 melange exporter
// magnus.osterlind@gmail.com
//-----------------------------------------------------------------------------

#include "exporter.hpp"
#include "exporter_helpers.hpp"
#include "boba_scene_format.hpp"
#include "deferred_writer.hpp"
#include "save_scene.hpp"
#include <memory>

//-----------------------------------------------------------------------------
namespace
{
  const float DEFAULT_NEAR_PLANE = 1.f;
  const float DEFAULT_FAR_PLANE = 1000.f;

  melange::AlienBaseDocument* g_Doc;
  melange::HyperFile* g_File;

  // Fixup functions called after the scene has been read and processed.
  vector<function<bool()>> g_deferredFunctions;
}

void CollectionAnimationTracks(melange::BaseList2D* bl, vector<Track>* tracks);

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

u32 Scene::nextObjectId = 1;

//-----------------------------------------------------------------------------
Scene g_scene;
Options options;

#define LOG(lvl, fmt, ...)                                                                         \
  if (options.loglevel >= lvl)                                                                    \
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
    melange::_MemFree((void**) &(c));
  }
  return res;
}

//-----------------------------------------------------------------------------
void CopyTransform(const melange::Matrix& mtx, Transform* xform)
{
  xform->pos = mtx.off;
  xform->rot = melange::MatrixToHPB(mtx, melange::ROTATIONORDER_HPB);
  xform->scale = melange::Vector(Len(mtx.v1), Len(mtx.v2), Len(mtx.v3));
}

//-----------------------------------------------------------------------------
void CopyMatrix(const melange::Matrix& mtx, float* out)
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
  melange::GeData data;
  obj->GetParameter(paramId, data);
  melange::Vector v = data.GetVector();
  return R(v.x, v.y, v.z);
}

//-----------------------------------------------------------------------------
template <typename T>
float GetFloatParam(T* obj, int paramId)
{
  melange::GeData data;
  obj->GetParameter(paramId, data);
  return (float)data.GetFloat();
}

//-----------------------------------------------------------------------------
template <typename T>
int GetInt32Param(T* obj, int paramId)
{
  melange::GeData data;
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
melange::Vector CalcNormal(const melange::Vector& a, const melange::Vector& b, const melange::Vector& c)
{
  melange::Vector e0 = (b - a);
  e0.Normalize();

  melange::Vector e1 = (c - a);
  e1.Normalize();

  return Cross(e0, e1);
}

//-----------------------------------------------------------------------------
void CollectVertices(melange::PolygonObject* polyObj, Mesh* mesh)
{
  // get point and polygon array pointer and counts
  const melange::Vector* verts = polyObj->GetPointR();
  const melange::CPolygon* polysOrg = polyObj->GetPolygonR();

  int vertexCount = polyObj->GetPointCount();
  if (!vertexCount)
    return;

  // calc bounding sphere (get center and max radius)
  melange::Vector center(verts[0]);
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
    Mesh::MaterialGroup mg;
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

  melange::Vector32* phongNormals = hasPhongTag ? polyObj->CreatePhongNormals() : nullptr;
  melange::NormalTag* normals =
      hasNormalsTag ? (melange::NormalTag*)polyObj->GetTag(Tnormal) : nullptr;
  melange::UVWTag* uvs = (melange::UVWTag*)polyObj->GetTag(Tuvw);
  melange::ConstUVWHandle uvHandle = uvs ? uvs->GetDataAddressR() : nullptr;

  // add verts
  mesh->verts.reserve(numVerts * 3);
  mesh->normals.reserve(numVerts * 3);
  mesh->indices.reserve(numVerts * 3);
  mesh->uv.reserve(numVerts * 3);

  melange::ConstNormalHandle normalHandle = normals ? normals->GetDataAddressR() : nullptr;

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
        melange::NormalStruct normal;
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
      melange::Vector normal = CalcNormal(verts[idx0], verts[idx1], verts[idx2]);
      Add3Vector3(&mesh->normals, normal, normal, normal);
      if (isQuad)
      {
        AddVector3(&mesh->normals, normal);
      }
    }

    if (uvHandle)
    {
      melange::UVWStruct s;
      melange::UVWTag::Get(uvHandle, i, s);

      Add3Vector2(&mesh->uv, s.a, s.b, s.c, true);
      if (isQuad)
      {
        // face 0, 2, 3
        AddVector2(&mesh->uv, s.d, true);
      }
    }
  }

  if (phongNormals)
    melange::_MemFree((void**)&phongNormals);
}

//-----------------------------------------------------------------------------
void CollectMaterials(melange::AlienBaseDocument* c4dDoc)
{
  // add default material
  g_scene.materials.push_back(new ::Material());
  ::Material* exporterMaterial = g_scene.materials.back();
  exporterMaterial->mat = nullptr;
  exporterMaterial->name = "<default>";
  exporterMaterial->id = ~0u;
  exporterMaterial->flags = ::Material::FLAG_COLOR;
  exporterMaterial->color.brightness = 1.f;
  exporterMaterial->color.color = Color(0.5f, 0.5f, 0.5f);

  for (melange::BaseMaterial* mat = c4dDoc->GetFirstMaterial(); mat; mat = mat->GetNext())
  {
    // check if the material is a standard material
    if (mat->GetType() != Mmaterial)
      continue;

    string name = CopyString(mat->GetName());

    g_scene.materials.push_back(new ::Material());
    ::Material* exporterMaterial = g_scene.materials.back();
    exporterMaterial->mat = mat;
    exporterMaterial->name = name;

    // check if the given channel is used in the material
    if (((melange::Material*)mat)->GetChannelState(CHANNEL_COLOR))
    {
      exporterMaterial->flags |= ::Material::FLAG_COLOR;
      exporterMaterial->color.brightness = GetFloatParam(mat, melange::MATERIAL_COLOR_BRIGHTNESS);
      exporterMaterial->color.color = GetVectorParam<Color>(mat, melange::MATERIAL_COLOR_COLOR);
    }

    if (((melange::Material*)mat)->GetChannelState(CHANNEL_REFLECTION))
    {
      exporterMaterial->flags |= Material::FLAG_REFLECTION;
      exporterMaterial->reflection.brightness = GetFloatParam(mat, melange::MATERIAL_REFLECTION_BRIGHTNESS);
      exporterMaterial->reflection.color =
          GetVectorParam<Color>(mat, melange::MATERIAL_REFLECTION_COLOR);
    }
  }
}

//-----------------------------------------------------------------------------
void CollectMeshMaterials(melange::PolygonObject* obj, Mesh* mesh)
{
  unordered_set<u32> selectedPolys;

  u32 prevMaterial = ~0u;
  u32 firstMaterial = ~0u;

  // For each material found, check if the following tag is a selection tag, in which
  // case record which polys belong to it
  for (melange::BaseTag* btag = obj->GetFirstTag(); btag; btag = btag->GetNext())
  {
    // texture tag
    if (btag->GetType() == Ttexture)
    {
      melange::GeData data;
      melange::AlienMaterial* mat = btag->GetParameter(melange::TEXTURETAG_MATERIAL, data)
                                        ? (melange::AlienMaterial*)data.GetLink()
                                        : NULL;
      if (!mat)
        continue;

      auto materialIt = find_if(RANGE(g_scene.materials),
          [mat](const Material* m)
          {
            return m->mat == mat;
          });
      if (materialIt == g_scene.materials.end())
        continue;

      Material* bobaMaterial = *materialIt;
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

      if (melange::BaseSelect* bs = ((melange::SelectionTag*)btag)->GetBaseSelect())
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
    u32 dummyMaterial = DEFAULT_MATERIAL;
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
        g.first == DEFAULT_MATERIAL ? "<default>" : g_scene.materials[g.first]->name.c_str();
    LOG(2, "material: %s, %d polys\n", materialName, (int)g.second.size());
  }
}

//-----------------------------------------------------------------------------
BaseObject::BaseObject(melange::BaseObject* melangeObj)
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
    valid = false;
  }

  g_scene.objMap[melangeObj] = this;
}

//-----------------------------------------------------------------------------
bool melange::AlienPrimitiveObjectData::Execute()
{
  melange::BaseObject* baseObj = (melange::BaseObject*)GetNode();

  string name(CopyString(baseObj->GetName()));
  LOG(1, "Skipping primitive object: %s\n", name.c_str());
  return true;
}

//-----------------------------------------------------------------------------
void CollectionAnimationTracks(melange::BaseList2D* bl, vector<Track>* tracks)
{
  if (!bl || !bl->GetFirstCTrack())
    return;

  for (melange::CTrack* ct = bl->GetFirstCTrack(); ct; ct = ct->GetNext())
  {
    // CTrack name
    Track track;
    track.name = CopyString(ct->GetName());

    // time track
    melange::CTrack* tt = ct->GetTimeTrack(bl->GetDocument());
    if (tt)
    {
      LOG(1, "Time track is unsupported");
    }

    // get DescLevel id
    melange::DescID testID = ct->GetDescriptionID();
    melange::DescLevel lv = testID[0];
    ct->SetDescriptionID(ct, testID);

    // get CCurve and print key frame data
    melange::CCurve* cc = ct->GetCurve();
    if (cc)
    {
      Curve curve;
      curve.name = CopyString(cc->GetName());

      for (int k = 0; k < cc->GetKeyCount(); k++)
      {
        melange::CKey* ck = cc->GetKey(k);
        melange::BaseTime t = ck->GetTime();
        if (ct->GetTrackCategory() == melange::PSEUDO_VALUE)
        {
          curve.keyframes.push_back(Keyframe{(int)t.GetFrame(g_Doc->GetFps()), ck->GetValue()});
        }
        else if (ct->GetTrackCategory() == melange::PSEUDO_PLUGIN && ct->GetType() == CTpla)
        {
          LOG(1, "Plugin keyframes are unsupported");
        }
        else if (ct->GetTrackCategory() == melange::PSEUDO_PLUGIN && ct->GetType() == CTmorph)
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
bool melange::AlienPolygonObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  PolygonObject* polyObj = (PolygonObject*)baseObj;

  Mesh* mesh = new Mesh(baseObj);
  CollectionAnimationTracks(baseObj, &mesh->animTracks);

  CollectMeshMaterials(polyObj, mesh);
  CollectVertices(polyObj, mesh);

#if WITH_XFORM_MTX
  CopyMatrix(polyObj->GetMl(), mesh->mtxLocal);
  CopyMatrix(polyObj->GetMg(), mesh->mtxGlobal);
#endif

  CopyTransform(polyObj->GetMl(), &mesh->xformLocal);
  CopyTransform(polyObj->GetMg(), &mesh->xformGlobal);

  if (mesh->valid)
  {
    g_scene.meshes.push_back(mesh);
  }
  else
  {
    delete mesh;
  }

  return true;
}

//-----------------------------------------------------------------------------
bool melange::AlienCameraObjectData::Execute()
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

  Camera* camera = new Camera(baseObj);

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
#if WITH_XFORM_MTX
  CopyMatrix(baseObj->GetMl(), camera->mtxLocal);
  CopyMatrix(baseObj->GetMg(), camera->mtxGlobal);
#endif

  CopyTransform(baseObj->GetMl(), &camera->xformLocal);
  CopyTransform(baseObj->GetMg(), &camera->xformGlobal);

  camera->verticalFov = GetFloatParam(baseObj, CAMERAOBJECT_FOV_VERTICAL);
  camera->nearPlane =
      GetInt32Param(baseObj, CAMERAOBJECT_NEAR_CLIPPING_ENABLE)
          ? max(DEFAULT_NEAR_PLANE, GetFloatParam(baseObj, CAMERAOBJECT_NEAR_CLIPPING))
          : DEFAULT_NEAR_PLANE;
  camera->farPlane = GetInt32Param(baseObj, CAMERAOBJECT_FAR_CLIPPING_ENABLE)
                         ? GetFloatParam(baseObj, CAMERAOBJECT_FAR_CLIPPING)
                         : DEFAULT_FAR_PLANE;

  if (camera->valid)
  {
    g_scene.cameras.push_back(camera);
  }
  else
  {
    delete camera;
  }
  return true;
}

//-----------------------------------------------------------------------------
void GetChildren(melange::BaseObject* obj, vector<melange::BaseObject*>* children)
{
  melange::BaseObject* child = obj->GetDown();
  while (child)
  {
    children->push_back(child);
    child = child->GetNext();
  }
}

//-----------------------------------------------------------------------------
void ExportSpline(melange::BaseObject* obj)
{
  melange::SplineObject* splineObject = static_cast<melange::SplineObject*>(obj);
  string splineName = CopyString(splineObject->GetName());

  melange::SPLINETYPE splineType = splineObject->GetSplineType();
  bool isClosed = splineObject->GetIsClosed();
  int pointCount = splineObject->GetPointCount();
  const melange::Vector* points = splineObject->GetPointR();

  Spline* s = new Spline(splineObject);
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
bool melange::AlienNullObjectData::Execute()
{
  melange::BaseObject* baseObj = (melange::BaseObject*)GetNode();

  const string name = CopyString(baseObj->GetName());

  NullObject* nullObject = new NullObject(baseObj);
  CollectionAnimationTracks(baseObj, &nullObject->animTracks);
#if WITH_XFORM_MTX
  CopyMatrix(baseObj->GetMl(), nullObject->mtxLocal);
  CopyMatrix(baseObj->GetMg(), nullObject->mtxGlobal);
#endif

  CopyTransform(baseObj->GetMl(), &nullObject->xformLocal);
  CopyTransform(baseObj->GetMg(), &nullObject->xformGlobal);

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
bool melange::AlienLightObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  const string name = CopyString(baseObj->GetName());

  int lightType = GetInt32Param(baseObj, LIGHT_TYPE);

  if (lightType == LIGHT_TYPE_OMNI)
  {
  }
  else if (lightType == LIGHT_TYPE_DISTANT)
  {
  }
  else if (lightType == LIGHT_TYPE_SPOT)
  {
  }
  else
  {
    LOG(1, "Unsupported light type: %s\n", name.c_str());
    return true;
  }
  
  Light* light = new Light(baseObj);
  CollectionAnimationTracks(baseObj, &light->animTracks);
#if WITH_XFORM_MTX
  CopyMatrix(baseObj->GetMl(), light->mtxLocal);
  CopyMatrix(baseObj->GetMg(), light->mtxGlobal);
#endif

  CopyTransform(baseObj->GetMl(), &light->xformLocal);
  CopyTransform(baseObj->GetMg(), &light->xformGlobal);

  light->type = lightType;
  light->color = GetVectorParam<Color>(baseObj, LIGHT_COLOR);
  light->intensity = GetFloatParam(baseObj, LIGHT_BRIGHTNESS);

  light->falloffType = GetInt32Param(baseObj, LIGHT_DETAILS_FALLOFF);
  // LIGHT_DETAILS_FALLOFF_LINEAR = 8,
  if (light->falloffType == LIGHT_DETAILS_FALLOFF_LINEAR)
  {
    light->falloffRadius = GetFloatParam(baseObj, LIGHT_DETAILS_OUTERDISTANCE);
  }

  if (lightType == LIGHT_TYPE_DISTANT)
  {
  }
  else if (lightType == LIGHT_TYPE_SPOT)
  {
    light->outerAngle = GetFloatParam(baseObj, LIGHT_DETAILS_OUTERANGLE);
  }

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
struct ArgParse
{
  void AddFlag(const char* shortName, const char* longName, bool* out)
  {
    handlers.push_back(new FlagHandler(shortName, longName, out));
  }

  void AddIntArgument(const char* shortName, const char* longName, int* out)
  {
    handlers.push_back(new IntHandler(shortName, longName, out));
  }

  void AddFloatArgument(const char* shortName, const char* longName, float* out)
  {
    handlers.push_back(new FloatHandler(shortName, longName, out));
  }

  void AddStringArgument(const char* shortName, const char* longName, string* out)
  {
    handlers.push_back(new StringHandler(shortName, longName, out));
  }

  bool Parse(int argc, char** argv)
  {
    // parse all keyword arguments
    int idx = 0;
    while (idx < argc)
    {
      string cmd = argv[idx];
      // oh noes, O(n), tihi
      BaseHandler* handler = nullptr;
      for (int i = 0; i < (int)handlers.size(); ++i)
      {
        if (cmd == handlers[i]->longName || cmd == handlers[i]->shortName)
        {
          handler = handlers[i];
          break;
        }
      }

      if (!handler)
      {
        // end of keyword arguments, so copy the remainder into the positional list
        for (int i = idx; i < argc; ++i)
          positional.push_back(argv[i]);
        return true;
      }

      if (argc - idx < handler->RequiredArgs())
      {
        error = string("Too few arguments left for: ") + argv[idx];
        return false;
      }

      const char* arg = handler->RequiredArgs() ? argv[idx + 1] : nullptr;
      if (!handler->Process(arg))
      {
        error = string("Error processing argument: ") + (arg ? arg : "");
        return false;
      }

      idx += handler->RequiredArgs() + 1;
    }

    return true;
  }

  void PrintHelp()
  {
    printf("Valid arguments are:\n");
  }

  struct BaseHandler
  {
    BaseHandler(const char* shortName, const char* longName)
        : shortName(shortName ? shortName : ""), longName(longName ? longName : "")
    {
    }
    virtual int RequiredArgs() = 0;
    virtual bool Process(const char* value) = 0;

    string shortName;
    string longName;
  };

  struct FlagHandler : public BaseHandler
  {
    FlagHandler(const char* shortName, const char* longName, bool* out)
        : BaseHandler(shortName, longName), out(out)
    {
    }
    virtual int RequiredArgs() override { return 0; }
    virtual bool Process(const char* value) override
    {
      *out = true;
      return true;
    }
    bool* out;
  };

  struct IntHandler : public BaseHandler
  {
    IntHandler(const char* shortName, const char* longName, int* out) : BaseHandler(shortName, longName), out(out) {}
    virtual int RequiredArgs() override { return 1; }
    virtual bool Process(const char* value) override
    {
      return sscanf(value, "%d", out) == 1;
    }
    int* out;
  };

  struct FloatHandler : public BaseHandler
  {
    FloatHandler(const char* shortName, const char* longName, float* out) : BaseHandler(shortName, longName), out(out) {}
    virtual int RequiredArgs() override { return 1; }
    virtual bool Process(const char* value) override
    {
      return sscanf(value, "%f", out) == 1;
    }
    float* out;
  };

  struct StringHandler : public BaseHandler
  {
    StringHandler(const char* shortName, const char* longName, string* out) : BaseHandler(shortName, longName), out(out) {}
    virtual int RequiredArgs() override { return 1; }
    virtual bool Process(const char* value) override
    {
      *out = value;
      return true;
    }
    string* out;
  };

  vector<string> positional;
  string error;
  vector<BaseHandler*> handlers;
};

u32 Material::nextId;

//------------------------------------------------------------------------------
#ifndef _WIN32
template <typename T, typename U>
constexpr size_t offsetOf(U T::*member)
{
  return (char*)&((T*)nullptr->*member) - (char*)nullptr;
}
#endif

//------------------------------------------------------------------------------
BaseObject* Scene::FindObject(melange::BaseObject* obj)
{
  auto it = objMap.find(obj);
  return it == objMap.end() ? nullptr : it->second;
}

//-----------------------------------------------------------------------------
void ExportAnimations()
{
  melange::GeData mydata;
  float startTime = 0.0, endTime = 0.0;
  int startFrame = 0, endFrame = 0, fps = 0;

  // get fps 
  if (g_Doc->GetParameter(melange::DOCUMENT_FPS, mydata))
    fps = mydata.GetInt32();

  // get start and end time 
  if (g_Doc->GetParameter(melange::DOCUMENT_MINTIME, mydata))
    startTime = mydata.GetTime().Get();

  if (g_Doc->GetParameter(melange::DOCUMENT_MAXTIME, mydata))
    endTime = mydata.GetTime().Get();

  // calculate start and end frame 
  startFrame = startTime * fps;
  endFrame = endTime * fps;

  float inc = 1.0f / fps;
  float curTime = startTime;
  
  while (curTime <= endTime)
  {
    g_Doc->SetTime(melange::BaseTime(curTime));
    g_Doc->Execute();

    curTime += inc;
  }
}

bool ParseFilenames(const vector<string>& args)
{
  int curArg = 0;
  int remaining = (int)args.size();

  const auto& step = [&curArg, &remaining](int steps)
  {
    curArg += steps;
    remaining -= steps;
  };

  if (remaining < 1)
  {
    printf("Invalid args\n");
    return 1;
  }

  options.inputFilename = MakeCanonical(args[0]);
  step(1);

  // create output file
  if (remaining == 1)
  {
    // check if the remaining argument is a file name, or just a directory
    if (strstr(args[curArg].c_str(), "boba") != nullptr)
    {
      options.outputFilename = args[curArg];
    }
    else
    {
      // a directory was given, so create the filename from the input file
      string res = FilenameFromInput(options.inputFilename, true);
      if (res.empty())
        return 1;

      options.outputFilename = string(args[curArg]) + '/' + res;
    }
  }
  else
  {
    options.outputFilename = FilenameFromInput(options.inputFilename, false);
    if (options.outputFilename.empty())
      return 1;
  }

  options.logfile = fopen((options.outputFilename + ".log").c_str(), "at");
  return true;
}

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  ArgParse parser;
  parser.AddFlag(nullptr, "compress-vertices", &options.compressVertices);
  parser.AddFlag(nullptr, "compress-indices", &options.compressIndices);
  parser.AddFlag(nullptr, "optimize-indices", &options.optimizeIndices);
  parser.AddIntArgument(nullptr, "loglevel", &options.loglevel);

  if (!parser.Parse(argc - 1, argv + 1))
  {
    fprintf(stderr, "%s", parser.error.c_str());
    return 1;
  }

  if (!ParseFilenames(parser.positional))
  {
    fprintf(stderr, "Error parsing filenames");
    return 1;
  }

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

  g_Doc = NewObj(melange::AlienBaseDocument);
  g_File = NewObj(melange::HyperFile);

  if (!g_File->Open(DOC_IDENT, options.inputFilename.c_str(), melange::FILEOPEN_READ))
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

  ExportAnimations();

  SceneStats stats;
  if (res)
  {
    SaveScene(g_scene, options, &stats);
  }

  DeleteObj(g_Doc);
  DeleteObj(g_File);

  LOG(2,
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

  time_t endTime = time(0);
  now = localtime(&endTime);

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
