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

//-----------------------------------------------------------------------------
using namespace melange;
using namespace boba;

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
void ExportMaterials(PolygonObject* obj, Mesh* mesh)
{
  GeData data;

  for (BaseTag* btag = obj->GetFirstTag(); btag; btag = (BaseTag*)btag->GetNext())
  {
    const char* tagName = btag->GetName().GetCStringCopy();
    if (tagName)
    {
      printf("   - %s \"%s\"\n", GetObjectTypeName(btag->GetType()), tagName);
      DeleteMem(tagName);
    }

    // texture tag
    if (btag->GetType() == Ttexture)
    {
      // detect the material
      AlienMaterial *mat = NULL;
      if (btag->GetParameter(TEXTURETAG_MATERIAL, data))
        mat = (AlienMaterial*)data.GetLink();
      if (mat)
      {
        char *pCharMat = NULL, *pCharSh = NULL;
        Vector col = Vector(0.0, 0.0, 0.0);
        if (mat->GetParameter(MATERIAL_COLOR_COLOR, data))
          col = data.GetVector();

        pCharMat = mat->GetName().GetCStringCopy();
        if (pCharMat)
        {
          printf(" - material: \"%s\" (%d/%d/%d)", pCharMat, int(col.x * 255), int(col.y * 255), int(col.z * 255));
          DeleteMem(pCharMat);
        }
        else
          printf(" - material: <noname> (%d/%d/%d)", int(col.x * 255), int(col.y * 255), int(col.z * 255));
        // detect the shader
        BaseShader* sh = mat->GetShader(MATERIAL_COLOR_SHADER);
        if (sh)
        {
          pCharSh = sh->GetName().GetCStringCopy();
          if (pCharSh)
          {
            printf(" - color shader \"%s\" - Type: %s", pCharSh, GetObjectTypeName(sh->GetType()));
            DeleteMem(pCharSh);
          }
          else
            printf(" - color shader <noname> - Type: %s", GetObjectTypeName(sh->GetType()));
        }
        else
          printf(" - no shader");
      }
      else
        printf(" - no material");
    }

    // Polygon Selection Tag
    if (btag->GetType() == Tpolygonselection && obj->GetType() == Opolygon)
    {
      printf("\n");
      BaseSelect *bs = ((SelectionTag*)btag)->GetBaseSelect();
      if (bs)
      {
        LONG s = 0;
        for (s = 0; s < ((PolygonObject*)obj)->GetPolygonCount() && s < 5; s++)
        {
          if (bs->IsSelected(s))
            printf("     Poly %d: selected\n", (int)s);
          else
            printf("     Poly %d: NOT selected\n", (int)s);
        }
        if (s < ((PolygonObject*)obj)->GetPolygonCount())
          printf("     ...\n");
      }
    }

    printf("\n");
  }
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  Mesh* mesh = new Mesh((u32)scene.meshes.size());

  // get object pointer
  PolygonObject* obj = (PolygonObject*)GetNode();

  const char* nameCStr = obj->GetName().GetCStringCopy();
  mesh->name = nameCStr;
  printf("Exporting: %s\n", nameCStr);
  DeleteMem(nameCStr);

  ExportVertices(obj, mesh);
  ExportMaterials(obj, mesh);

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
class AlienBaseDocument : public BaseDocument
{
public:
  virtual Bool Execute() 
  { 
    return true; 
  }
};

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
