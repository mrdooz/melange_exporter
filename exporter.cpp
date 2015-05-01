//-----------------------------------------------------------------------------
// Cinema4 melange exporter
// magnus.osterlind@gmail.com
//-----------------------------------------------------------------------------

#include "exporter.hpp"
#include "exporter_helpers.hpp"
#include "boba_io.hpp"

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

#define LOG(lvl, fmt, ...) \
if (options.verbosity >= lvl) { printf(fmt, __VA_ARGS__); }

//-----------------------------------------------------------------------------
using namespace melange;
using namespace boba;
using namespace std;

#define RANGE(c) (c).begin(), (c).end()

struct Options
{
  string inputFilename;
  string outputFilename;
  bool makeFaceted = false;
  int verbosity = 0;
};

Scene scene;
Options options;

//-----------------------------------------------------------------------------
RootMaterial *AllocAlienRootMaterial()			{ return NewObj(RootMaterial); }
RootObject *AllocAlienRootObject()					{ return NewObj(RootObject); }
RootLayer *AllocAlienRootLayer()						{ return NewObj(RootLayer); }
RootRenderData *AllocAlienRootRenderData()	{ return NewObj(RootRenderData); }
RootViewPanel *AllocC4DRootViewPanel()			{ return NewObj(RootViewPanel); }
LayerObject *AllocAlienLayer()							{ return NewObj(LayerObject); }

//-----------------------------------------------------------------------------
Bool AlienMaterial::Execute()
{
  Print();
  return true;
}

//-----------------------------------------------------------------------------
Bool AlienLayer::Execute()
{
  Print();
  return true;
}

//-----------------------------------------------------------------------------
Bool AlienPrimitiveObjectData::Execute()
{
  // decide the object type
  if (type_id == Ocube)
  {
    // get the size of the cube
    GeData cubeData;
    GetNode()->GetParameter(PRIM_CUBE_LEN, cubeData);
    Vector xyzLength = cubeData.GetVector();
    // create your own parametric cube with length X, Y and Z
    // ...
    // returning TRUE we will NOT get another call of PolygonObjectData Execute()
    // HACK! returning false means the object wasn't processed
    return false;
  }
  // return FALSE to get a simpler geometric description of objects like Osphere, Ocone, ...
  // the Execute() of AlienPolygonObjectData will be called
  return false;
}

//-----------------------------------------------------------------------------
class AlienBaseDocument : public BaseDocument
{
public:
  virtual Bool Execute()
  {
    return true;
  }
};

//-----------------------------------------------------------------------------
template<typename T>
float* AddVector(float* f, const T& v)
{
  *f++ = v.x;
  *f++ = v.y;
  *f++ = v.z;
  return f;
}

//-----------------------------------------------------------------------------
void ExportVertices(PolygonObject* obj, Mesh* mesh)
{
  // get point and polygon array pointer and counts
  const Vector* verts = obj->GetPointR();

  const CPolygon* polys = obj->GetPolygonR();
  u32 numPolys = obj->GetPolygonCount();
  u32 numNGons = obj->GetNgonCount();

  u32 numVerts = 0;
  for (u32 i = 0; i < numPolys; ++i)
  {
    // if the polygon is a quad, we're going to double the shared verts
    if (polys[i].c == polys[i].d)
      numVerts += 3;
    else
      numVerts += options.makeFaceted ? 2 * 3 : 4;
  }

  // Check what kind of normals exist
  bool hasNormalsTag = !!obj->GetTag(Tnormal);
  bool hasPhongTag = !!obj->GetTag(Tphong);
  bool hasNormals = hasNormalsTag || hasPhongTag;

  Vector32* phongNormals = hasPhongTag ? obj->CreatePhongNormals() : nullptr;
  NormalTag* normals = hasNormalsTag ? (NormalTag*)obj->GetTag(Tnormal) : nullptr;


  // add verts
  mesh->verts.resize(numVerts * 3);
  mesh->indices.reserve(numVerts * 3);

  if (hasNormals)
    mesh->normals.resize(numVerts * 3);

  ConstNormalHandle normalHandle = normals ? normals->GetDataAddressR() : nullptr;

  float* v = mesh->verts.data();
  float* n = mesh->normals.data();
  u32 vertOfs = 0;

  for (u32 i = 0; i < numPolys; ++i)
  {
    int idx0 = polys[i].a;
    int idx1 = polys[i].b;
    int idx2 = polys[i].c;
    int idx3 = polys[i].d;
    bool isQuad = idx2 != idx3;

    // face 0, 1, 2
    v = AddVector(v, verts[idx0]);
    v = AddVector(v, verts[idx1]);
    v = AddVector(v, verts[idx2]);

    mesh->indices.push_back(vertOfs + 0);
    mesh->indices.push_back(vertOfs + 1);
    mesh->indices.push_back(vertOfs + 2);

    if (isQuad)
    {
      // face 0, 2, 3
      if (options.makeFaceted)
      {
        // if making a faceted mesh, duplicate the shared vertices
        v = AddVector(v, verts[idx0]);
        v = AddVector(v, verts[idx2]);
        v = AddVector(v, verts[idx3]);

        mesh->indices.push_back(vertOfs + 3 + 0);
        mesh->indices.push_back(vertOfs + 3 + 1);
        mesh->indices.push_back(vertOfs + 3 + 2);
        vertOfs += 6;
      }
      else
      {
        v = AddVector(v, verts[idx3]);

        mesh->indices.push_back(vertOfs + 0);
        mesh->indices.push_back(vertOfs + 2);
        mesh->indices.push_back(vertOfs + 3);
        vertOfs += 4;
      }
    }

    if (hasNormals)
    {
      if (hasNormalsTag)
      {
        NormalStruct normal;
        normals->Get(normalHandle, i, normal);
        n = AddVector(n, normal.a);
        n = AddVector(n, normal.b);
        n = AddVector(n, normal.c);

        if (isQuad)
        {
          if (options.makeFaceted)
          {
            n = AddVector(n, normal.a);
            n = AddVector(n, normal.c);
            n = AddVector(n, normal.d);
          }
          else
          {
            n = AddVector(n, normal.d);
          }
        }
      }
      else if (hasPhongTag)
      {
        n = AddVector(n, phongNormals[i * 4 + 0]);
        n = AddVector(n, phongNormals[i * 4 + 1]);
        n = AddVector(n, phongNormals[i * 4 + 2]);

        if (isQuad)
        {
          if (options.makeFaceted)
          {
            n = AddVector(n, phongNormals[i * 4 + 0]);
            n = AddVector(n, phongNormals[i * 4 + 2]);
            n = AddVector(n, phongNormals[i * 4 + 3]);
          }
          else
          {
            n = AddVector(n, phongNormals[i * 4 + 3]);
          }
        }
      }
    }
  }

  if (phongNormals)
    DeleteMem(phongNormals);
}

//-----------------------------------------------------------------------------
string CopyString(const String& str)
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
void ExportMaterials(AlienBaseDocument* c4dDoc)
{
  // get the first material from the document and go through the whole list
  BaseMaterial* mat = c4dDoc->GetFirstMaterial();
  while (mat)
  {
    //printf("mat: %p\n", mat);
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
        exporterMaterial.color.color = GetVectorParam<Color>(mat, MATERIAL_COLOR_COLOR);
      }

      if (((melange::Material*)mat)->GetChannelState(CHANNEL_REFLECTION))
      {
        exporterMaterial.flags |= boba::Material::FLAG_REFLECTION;

        exporterMaterial.reflection.brightness = GetFloatParam(mat, MATERIAL_REFLECTION_BRIGHTNESS);
        exporterMaterial.reflection.color = GetVectorParam<Color>(mat, MATERIAL_REFLECTION_COLOR);
      }

    }

    mat = mat->GetNext();
  }
}

//-----------------------------------------------------------------------------
void ExportMeshMaterials(PolygonObject* obj, Mesh* mesh)
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
      // add the selected polygons to the last material group
      if (prevMaterial == ~0u)
      {
        printf("Selection tag found without material group!\n");
        continue;
      }

      if (BaseSelect *bs = ((SelectionTag*)btag)->GetBaseSelect())
      {
        Mesh::MaterialGroup &g = mesh->materialGroups[prevMaterial];
        for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
        {
          if (bs->IsSelected(i))
          {
            g.polys.push_back(i);
            selectedPolys.insert(i);
          }
        }
      }
    }
  }

  // add all the polygons that aren't selected to the first material
  if (firstMaterial != ~0u)
  {
    Mesh::MaterialGroup &g = mesh->materialGroups[firstMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      if (selectedPolys.count(i) == 0)
        g.polys.push_back(i);
    }
  }

  if (options.verbosity >= 2)
  {
    for (auto g : mesh->materialGroups)
    {
      printf("material: %s, %d polys\n", scene.materials[g.first].name.c_str(), (int)g.second.polys.size());
    }
  }
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  Mesh* mesh = new Mesh((u32)scene.meshes.size());

  // get object pointer
  PolygonObject* obj = (PolygonObject*)GetNode();

  string meshName = CopyString(obj->GetName());
  mesh->name = meshName;
  LOG(1, "Exporting: %s\n", meshName.c_str());

  ExportMeshMaterials(obj, mesh);
  ExportVertices(obj, mesh);

  scene.meshes.push_back(mesh);
  return true;
}

//-----------------------------------------------------------------------------
NodeData *AllocAlienObjectData(Int32 id, Bool &known)
{
  NodeData *m_data = NULL;
  known = true;
  switch (id)
  {
    // supported element types
    case Opolygon:        m_data = NewObj(AlienPolygonObjectData); break;
    case Osphere: m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocube: m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Oplane: m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocone: m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Otorus: m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocylinder: m_data = NewObj(AlienPrimitiveObjectData, id); break;

    default:              known = false; break;
  }

  return m_data;
}

//-----------------------------------------------------------------------------
// allocate the plugin tag elements data (only few tags have additional data which is stored in a NodeData)
NodeData *AllocAlienTagData(Int32 id, Bool &known)
{
  return 0;
}


//-----------------------------------------------------------------------------
// create objects, material and layers for the new C4D scene file
Bool BaseDocument::CreateSceneToC4D(Bool selectedonly)
{
  return true;
}


//-----------------------------------------------------------------------------
int ParseOptions(int argc, char** argv)
{
  // format is [--faceted] [--verbose N] input [output]

  if (argc < 2)
  {
    printf("No input file specified\n");
    return 1;
  }

  int inputArg = 1;
  int curArg = 1;
  int remaining = argc - 1;

  auto step = [&curArg, &remaining](int steps) { curArg += steps; remaining -= steps; };

  while (remaining)
  {
    if (strcmp(argv[curArg], "--faceted") == 0)
    {
      options.makeFaceted = true;
      step(1);
    }
    else if (strcmp(argv[curArg], "--verbose") == 0)
    {
      // we need at least 1 more argument, for the level
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

  return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  if (int res = ParseOptions(argc, argv))
    return res;

  AlienBaseDocument *C4Ddoc = NewObj(AlienBaseDocument);
  HyperFile *C4Dfile = NewObj(HyperFile);

  if (!C4Dfile->Open(DOC_IDENT, options.inputFilename.c_str(), FILEOPEN_READ))
    return 1;

  if (!C4Ddoc->ReadObject(C4Dfile, true))
    return 1;

  C4Dfile->Close();
  ExportMaterials(C4Ddoc);

  C4Ddoc->CreateSceneFromC4D();

  BaseMaterial* mat = C4Ddoc->GetFirstMaterial();
  while (mat)
  {
    String name = mat->GetName();
    mat = mat->GetNext();
  }

  scene.Save(options.outputFilename.c_str());

  DeleteObj(C4Ddoc);
  DeleteObj(C4Dfile);

  return 0;
}
