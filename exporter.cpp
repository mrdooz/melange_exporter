//-----------------------------------------------------------------------------
// Cinema4 melange exporter
// magnus.osterlind@gmail.com
//-----------------------------------------------------------------------------

#include "exporter.hpp"
#include "exporter_helpers.hpp"
#include "exporter_stubs.hpp"
#include "boba_scene.hpp"
#include "deferred_writer.hpp"

#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>
#include <stdio.h>
#include <assert.h>
#include <set>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include <time.h>

//-----------------------------------------------------------------------------
using namespace melange;
using namespace std;

//-----------------------------------------------------------------------------
RootMaterial *AllocAlienRootMaterial()			{ return NewObj(RootMaterial); }
RootObject *AllocAlienRootObject()					{ return NewObj(RootObject); }
RootLayer *AllocAlienRootLayer()						{ return NewObj(RootLayer); }
RootRenderData *AllocAlienRootRenderData()	{ return NewObj(RootRenderData); }
RootViewPanel *AllocC4DRootViewPanel()			{ return NewObj(RootViewPanel); }
LayerObject *AllocAlienLayer()							{ return NewObj(LayerObject); }

//-----------------------------------------------------------------------------
NodeData *AllocAlienObjectData(Int32 id, Bool &known)
{
  NodeData *m_data = NULL;
  switch (id)
  {
    // supported element types
    case Opolygon:  m_data = NewObj(AlienPolygonObjectData); break;
    case Ocamera:   m_data = NewObj(AlienCameraObjectData); break;
    case Osphere:   m_data = NewObj(AlienPrimitiveObjectData); break;
    case Ocube:     m_data = NewObj(AlienPrimitiveObjectData); break;
    case Oplane:    m_data = NewObj(AlienPrimitiveObjectData); break;
    case Ocone:     m_data = NewObj(AlienPrimitiveObjectData); break;
    case Otorus:    m_data = NewObj(AlienPrimitiveObjectData); break;
    case Ocylinder: m_data = NewObj(AlienPrimitiveObjectData); break;
  }

  known = !!m_data;
  return m_data;
}

//-----------------------------------------------------------------------------
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
}

namespace
{
  const u32 DEFAULT_MATERIAL = ~0u;
}

u32 boba::Scene::nextObjectId = 1;

//-----------------------------------------------------------------------------
boba::Scene scene;
boba::Options options;

#define LOG(lvl, fmt, ...)                             \
  if (options.verbosity >= lvl)                        \
    printf(fmt, __VA_ARGS__);                          \
  if (options.logfile)                                 \
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
  out[0] = (float)mtx.v1.x;    out[1] = (float)mtx.v1.y;    out[2] = (float)mtx.v1.z;
  out[3] = (float)mtx.v2.x;    out[4] = (float)mtx.v2.y;    out[5] = (float)mtx.v2.z;
  out[6] = (float)mtx.v3.x;    out[7] = (float)mtx.v3.y;    out[8] = (float)mtx.v3.z;
  out[9] = (float)mtx.off.x;   out[10] = (float)mtx.off.y;  out[11] = (float)mtx.off.z;
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
inline bool IsQuad(const T& p)
{
  return p.c != p.d;
}

//-----------------------------------------------------------------------------
void AddIndices(vector<int>* indices, int a, int b, int c)
{
  indices->push_back(a); indices->push_back(b); indices->push_back(c);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddVector(vector<float>* verts, const T& v)
{
  verts->push_back(v.x); verts->push_back(v.y); verts->push_back(v.z);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddTriangle(vector<float>* verts, const T& a, const T& b, const T& c)
{
  AddVector(verts, a); AddVector(verts, b); AddVector(verts, c);
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
void CollectVertices(PolygonObject* obj, boba::Mesh* mesh)
{
  // get point and polygon array pointer and counts
  const Vector* verts = obj->GetPointR();
  const CPolygon* polysOrg = obj->GetPolygonR();

  int vertexCount = obj->GetPointCount();
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
    float cand = (center - verts[1]).GetSquaredLength();
    radius = max(radius, cand);
  }

  mesh->boundingSphere.center = center;
  mesh->boundingSphere.radius = sqrtf(radius);

  // Loop over all the materials, and add the polygons in the
  // order they appear per material
  struct Polygon { int a, b, c, d; };
  vector<Polygon> polys;
  polys.reserve(obj->GetPolygonCount());

  int numPolys = 0;
  int idx = 0;
  for (auto kv : mesh->polysByMaterial)
  {
    boba::Mesh::MaterialGroup mg;
    mg.materialId = kv.first;
    mg.startIndex = idx;
    numPolys += (int)kv.second.size();

    for (int i : kv.second)
    {
      polys.push_back({ polysOrg[i].a, polysOrg[i].b, polysOrg[i].c, polysOrg[i].d });

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
    // if the polygon is a quad, we're going to double the shared verts
    numVerts += (options.shareVertices ? 1 : 2) * (IsQuad(polys[i]) ? 4 : 3);
  }

  // Check what kind of normals exist
  bool hasNormalsTag = !!obj->GetTag(Tnormal);
  bool hasPhongTag = !!obj->GetTag(Tphong);
  bool hasNormals = hasNormalsTag || hasPhongTag;

  Vector32* phongNormals = hasPhongTag ? obj->CreatePhongNormals() : nullptr;
  NormalTag* normals = hasNormalsTag ? (NormalTag*)obj->GetTag(Tnormal) : nullptr;

  // add verts
  mesh->verts.reserve(numVerts * 3);
  mesh->normals.reserve(numVerts * 3);
  mesh->indices.reserve(numVerts * 3);

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
    AddTriangle(&mesh->verts, verts[idx0], verts[idx1], verts[idx2]);
    AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 1, vertOfs + 2);

    if (isQuad)
    {
      // face 0, 2, 3
      if (options.shareVertices)
      {
        AddVector(&mesh->verts, verts[idx3]);
        AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 2, vertOfs + 3);
        vertOfs += 4;
      }
      else
      {
        // if making a faceted mesh, duplicate the shared vertices
        AddTriangle(&mesh->verts, verts[idx0], verts[idx1], verts[idx2]);
        AddIndices(&mesh->indices, vertOfs + 3 + 0, vertOfs + 3 + 1, vertOfs + 3 + 2);
        vertOfs += 6;
      }
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
        AddTriangle(&mesh->normals, normal.a, normal.b, normal.c);

        if (isQuad)
        {
          if (options.shareVertices)
            AddVector(&mesh->normals, normal.d);
          else
            AddTriangle(&mesh->normals, normal.a, normal.c, normal.d);
        }
      }
      else if (hasPhongTag)
      {
        AddTriangle(&mesh->normals, phongNormals[i * 4 + 0], phongNormals[i * 4 + 1], phongNormals[i * 4 + 2]);

        if (isQuad)
        {
          if (options.shareVertices)
            AddVector(&mesh->normals, phongNormals[i * 4 + 3]);
          else
            AddTriangle(&mesh->normals, phongNormals[i * 4 + 0], phongNormals[i * 4 + 2], phongNormals[i * 4 + 3]);
        }
      }
    }
    else
    {
      // no normals, so generate polygon normals and use them for all verts
      Vector normal = CalcNormal(verts[idx0], verts[idx1], verts[idx2]);
      AddTriangle(&mesh->normals, normal, normal, normal);
      if (isQuad)
      {
        if (options.shareVertices)
          AddVector(&mesh->normals, normal);
        else
          AddTriangle(&mesh->normals, normal, normal, normal);
      }
    }
  }

  if (phongNormals)
    DeleteMem(phongNormals);
}

//-----------------------------------------------------------------------------
void CollectMaterials(AlienBaseDocument* c4dDoc)
{
  // get the first material from the document and go through the whole list
  BaseMaterial* mat = c4dDoc->GetFirstMaterial();
  while (mat)
  {
    // basic data
    String name = mat->GetName();

    // check if the material is a standard material
    if (mat->GetType() == Mmaterial)
    {
      scene.materials.push_back(boba::Material());
      boba::Material& exporterMaterial = scene.materials.back();
      exporterMaterial.mat = mat;
      exporterMaterial.name = CopyString(name);

      // check if the given channel is used in the material
      if (((melange::Material*)mat)->GetChannelState(CHANNEL_COLOR))
      {
        exporterMaterial.flags |= boba::Material::FLAG_COLOR;

        exporterMaterial.color.brightness = GetFloatParam(mat, MATERIAL_COLOR_BRIGHTNESS);
        exporterMaterial.color.color = GetVectorParam<boba::Color>(mat, MATERIAL_COLOR_COLOR);
      }

      if (((melange::Material*)mat)->GetChannelState(CHANNEL_REFLECTION))
      {
        exporterMaterial.flags |= boba::Material::FLAG_REFLECTION;

        exporterMaterial.reflection.brightness = GetFloatParam(mat, MATERIAL_REFLECTION_BRIGHTNESS);
        exporterMaterial.reflection.color = GetVectorParam<boba::Color>(mat, MATERIAL_REFLECTION_COLOR);
      }

    }

    mat = mat->GetNext();
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
      AlienMaterial *mat = btag->GetParameter(TEXTURETAG_MATERIAL, data) ? (AlienMaterial *) data.GetLink() : NULL;
      if (!mat)
        continue;

      auto materialIt = find_if(RANGE(scene.materials), [mat](const boba::Material &m) { return m.mat == mat; });
      if (materialIt == scene.materials.end())
        continue;

      boba::Material &bobaMaterial = *materialIt;
      prevMaterial = bobaMaterial.id;
      if (firstMaterial == ~0u)
        firstMaterial = bobaMaterial.id;
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

      if (BaseSelect *bs = ((SelectionTag*)btag)->GetBaseSelect())
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
    const char* materialName = g.first == DEFAULT_MATERIAL ? "<default>" : scene.materials[g.first].name.c_str();
    LOG(2, "material: %s, %d polys\n", materialName, (int)g.second.size());
  }
}

//-----------------------------------------------------------------------------
boba::BaseObject::BaseObject(melange::BaseObject* melangeObj) 
  : melangeObj(melangeObj)
{
  parent = scene.FindObject(melangeObj->GetUp());
  name = CopyString(melangeObj->GetName());
  id = Scene::nextObjectId++;

  LOG(1, "Exporting: %s\n", name.c_str());
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  PolygonObject* obj = (PolygonObject*)baseObj;

  boba::Mesh* mesh = new boba::Mesh(baseObj);

  CollectMeshMaterials(obj, mesh);
  CollectVertices(obj, mesh);

  CopyMatrix(obj->GetMl(), mesh->mtx);
  scene.meshes.push_back(mesh);

  return true;
}

//-----------------------------------------------------------------------------
Bool AlienCameraObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();

  // we require the parent object to be a null object
  BaseObject* parent = baseObj->GetUp();
  bool isNullParent = parent && parent->GetType() == OBJECT_NULL;
  if (!isNullParent)
  {
    LOG(1, "Camera's %s parent isn't a null object!", CopyString(baseObj->GetName()).c_str());
    return true;
  }

  boba::Camera* camera = new boba::Camera(baseObj);
  CopyMatrix(parent->GetMl(), camera->mtx);

  GeData data;
  baseObj->GetParameter(CAMERAOBJECT_FOV_VERTICAL, data);
  camera->verticalFov = data.GetFloat();

  scene.cameras.push_back(camera);

  return true;
}

//-----------------------------------------------------------------------------
bool ExportLight(BaseObject* baseObj, BaseObject* parentObj)
{
  return true;
}

//-----------------------------------------------------------------------------
bool ExportNullObject(BaseObject* baseObj, BaseObject* parentObj)
{
  return true;
}


//-----------------------------------------------------------------------------
int ParseOptions(int argc, char** argv)
{
  // format is [--dont-share-vertices] [--verbose N] input [output]

  if (argc < 2)
  {
    printf("No input file specified\n");
    return 1;
  }

  int curArg = 1;
  int remaining = argc - 1;

  const auto& step = [&curArg, &remaining](int steps) { curArg += steps; remaining -= steps; };

  while (remaining)
  {
    if (strcmp(argv[curArg], "--dont-share-vertice") == 0)
    {
      options.shareVertices = false;
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
      options.verbosity = atoi(argv[curArg+1]);
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

  options.inputFilename = argv[curArg];
  step(1);

  // create output file
  if (remaining == 1)
  {
    options.outputFilename = argv[curArg];
  }
  else
  {
    if (const char* dot = strchr(options.inputFilename.c_str(), '.'))
    {
      int len = dot - options.inputFilename.c_str();
      options.outputFilename = options.inputFilename.substr(0, len) + ".boba";
    }
    else
    {
      printf("Invalid input filename given: %s\n", options.inputFilename.c_str());
      return 1;
    }
  }

  options.logfile = fopen((options.outputFilename + ".log").c_str(), "at");

  return 0;
}

u32 boba::Material::nextId;

//------------------------------------------------------------------------------
#ifndef _WIN32
template<typename T, typename U> constexpr size_t offsetOf(U T::*member)
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

//------------------------------------------------------------------------------
bool boba::Scene::Save(const Options& options)
{
  boba::DeferredWriter writer;
  if (!writer.Open(options.outputFilename.c_str()))
    return false;

  boba::SceneBlob header;
  memset(&header, 0, sizeof(header));
  header.id[0] = 'b';
  header.id[1] = 'o';
  header.id[2] = 'b';
  header.id[3] = 'a';

  // dummy write the header
  writer.Write(header);

  header.numMeshes = (u32)meshes.size();
  header.meshDataStart = writer.GetFilePos();
  for (Mesh* mesh : meshes)
    mesh->Save(writer);

  header.numLights = (u32)lights.size();
  header.lightDataStart = header.numLights ? writer.GetFilePos() : 0;
  for (Light* light : lights)
    light->Save(writer);

  header.numCameras = (u32)cameras.size();
  header.cameraDataStart = header.numCameras ? writer.GetFilePos() : 0;
  for (Camera* camera : cameras)
    camera->Save(writer);

  header.numMaterials = (u32)materials.size();
  header.materialDataStart = header.numMaterials ? writer.GetFilePos() : 0;
  for (Material& material : materials)
    material.Save(writer);

  header.fixupOffset = (u32)writer.GetFilePos();
  writer.WriteDeferredData();

  // write back the correct header
  writer.SetFilePos(0);
  writer.Write(header);

  return true;
}

//------------------------------------------------------------------------------
void boba::Material::Save(DeferredWriter& writer)
{
  writer.StartBlockMarker();

  writer.AddDeferredString(name);
  writer.Write(id);
  writer.Write(flags);

  u32 colorFixup = (flags & FLAG_COLOR) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  u32 luminanceFixup = (flags & FLAG_LUMINANCE) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  u32 reflectionFixup = (flags & FLAG_REFLECTION) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  if (flags & FLAG_COLOR)
  {
    writer.InsertFixup(colorFixup);
    writer.Write(color.color);
    writer.AddDeferredString(color.texture);
    writer.Write(color.brightness);
  }

  if (flags & FLAG_LUMINANCE)
  {
    writer.InsertFixup(luminanceFixup);
    writer.Write(luminance.color);
    writer.AddDeferredString(luminance.texture);
    writer.Write(luminance.brightness);
  }

  if (flags & FLAG_REFLECTION)
  {
    writer.InsertFixup(reflectionFixup);
    writer.Write(reflection.color);
    writer.AddDeferredString(reflection.texture);
    writer.Write(reflection.brightness);
  }

  writer.EndBlockMarker();
}

//------------------------------------------------------------------------------
void boba::Light::Save(DeferredWriter& writer)
{

}

//------------------------------------------------------------------------------
void boba::Camera::Save(DeferredWriter& writer)
{

}

//------------------------------------------------------------------------------
void boba::Mesh::Save(boba::DeferredWriter& writer)
{
  writer.AddDeferredString(name);

  // Note, divide by 3 here to write # verts, and not # floats
  writer.Write((u32)verts.size() / 3);
  writer.Write((u32)indices.size());

  // write the material groups
  writer.Write((u32)materialGroups.size());
  writer.AddDeferredVector(materialGroups);

  writer.AddDeferredVector(verts);
  writer.AddDeferredVector(normals);
  writer.AddDeferredVector(uv);

  writer.AddDeferredVector(indices);

  writer.Write(mtx);

  // save bounding box
  writer.Write(boundingSphere);
}


//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  if (int res = ParseOptions(argc, argv))
    return res;

  time_t t = time(0);   // get time now
  struct tm* now = localtime(&t);

  LOG(1, "=========================================================\n[%.4d:%.2d:%.2d-%.2d:%.2d:%.2d] %s -> %s\n",
    now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec,
    options.inputFilename.c_str(), options.outputFilename.c_str());
  
  AlienBaseDocument *C4Ddoc = NewObj(AlienBaseDocument);
  HyperFile *C4Dfile = NewObj(HyperFile);

  if (!C4Dfile->Open(DOC_IDENT, options.inputFilename.c_str(), FILEOPEN_READ))
    return 1;

  if (!C4Ddoc->ReadObject(C4Dfile, true))
    return 1;

  C4Dfile->Close();

  CollectMaterials(C4Ddoc);
  C4Ddoc->CreateSceneFromC4D();
  scene.Save(options);

  DeleteObj(C4Ddoc);
  DeleteObj(C4Dfile);

  if (options.logfile)
    fclose(options.logfile);

  return 0;
}
