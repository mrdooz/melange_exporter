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
template<typename T>
float* AddVector(float* f, const T& v)
{
  *f++ = v.x;
  *f++ = v.y;
  *f++ = v.z;
  return f;
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  Mesh* mesh = new Mesh((u32)scene.meshes.size());

  // get object pointer
  PolygonObject* obj = (PolygonObject*)GetNode();

  // get point and polygon array pointer and counts
  const Vector* verts = obj->GetPointR();
  //u32 numVerts = obj->GetPointCount();

  const CPolygon* polys = obj->GetPolygonR();
  u32 numPolys = obj->GetPolygonCount();
  u32 numNGons = obj->GetNgonCount();

  // count # vertices. we create unique vertice per face.
  // TODO: make this a flag
  // NOTE: ok, this isn't really true. we create unique vertices per polygon, and
  // if we triangulate, we're sharing
  u32 numVerts = 0;
  for (u32 i = 0; i < numPolys; ++i)
    numVerts += polys[i].c == polys[i].d ? 3 : 4;

  // Check what kind of normals exist
  bool hasNormalsTag = !!obj->GetTag(Tnormal);
  bool hasPhongTag = !!obj->GetTag(Tphong);
  bool hasNormals = hasNormalsTag || hasPhongTag;

  Vector32* phongNormals = hasPhongTag ? obj->CreatePhongNormals() : nullptr;
  NormalTag* normals = hasNormalsTag ? (NormalTag*)obj->GetTag(Tnormal) : nullptr;

  const char* nameCStr = obj->GetName().GetCStringCopy();
  mesh->name = nameCStr;
  DeleteMem(nameCStr);

  map<pair<Vector32, Vector32>, int> vv;  

  // add verts
  mesh->verts.resize(numVerts*3);
  mesh->indices.reserve(numVerts*3);

  if (hasNormals)
    mesh->normals.resize(numVerts*3);

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

    v = AddVector(v, verts[idx0]);
    v = AddVector(v, verts[idx1]);
    v = AddVector(v, verts[idx2]);

    mesh->indices.push_back(vertOfs+0);
    mesh->indices.push_back(vertOfs+1);
    mesh->indices.push_back(vertOfs+2);

    bool isQuad = idx2 != idx3;

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
          n = AddVector(n, normal.d);
      }
      else if (hasPhongTag)
      {
        n = AddVector(n, phongNormals[idx0]);
        n = AddVector(n, phongNormals[idx1]);
        n = AddVector(n, phongNormals[idx2]);

        if (isQuad)
          n = AddVector(n, phongNormals[idx3]);
      }
    }

    if (isQuad)
    {
      v = AddVector(v, verts[idx3]);

      mesh->indices.push_back(vertOfs+0);
      mesh->indices.push_back(vertOfs+2);
      mesh->indices.push_back(vertOfs+3);
      vertOfs++;
    }

    vertOfs += 3;
  }

  scene.meshes.push_back(mesh);
  //Print();
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
    default:              known = false; break;
  }

  return m_data;
}

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
  if (remaining > 1)
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
  scene.Save(options.outputFilename.c_str());

  DeleteObj(C4Ddoc);
  DeleteObj(C4Dfile);

  return 0;
}
