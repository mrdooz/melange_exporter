//-----------------------------------------------------------------------------
// Cinema4 melange exporter
// magnus.osterlind@gmail.com
//-----------------------------------------------------------------------------

#include "exporter.hpp"
#include "boba_io.hpp"

#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>
#include <stdio.h>
#include <assert.h>

//-----------------------------------------------------------------------------
namespace _melange_
{
  // memory allocation functions inside _melange_ namespace (if you have your own memory management you can overload these functions)
  // alloc memory no clear
  void *MemAllocNC(VLONG size)
  {
    void *mem = malloc(size);
    return mem;
  }

  // alloc memory set to 0
  void *MemAlloc(VLONG size)
  {
    void *mem = MemAllocNC(size);
    memset(mem, 0, size);
    return mem;
  }

  // realloc existing memory
  void *MemRealloc(void* orimem, VLONG size)
  {
    void *mem = realloc(orimem, size);
    return mem;
  }

  // free memory and set pointer to null
  void MemFree(void *&mem)
  {
    if (!mem)
      return;

    free(mem);
    mem = NULL;
  }
}

using namespace _melange_;
using namespace boba;

// overload this function and fill in your own unique data
void GetWriterInfo(LONG &id, String &appname)
{
  // register your own pluginid once for your exporter and enter it here under id
  // this id must be used for your own unique ids
  // 	Bool AddUniqueID(LONG appid, const CHAR *const mem, LONG bytes);
  // 	Bool FindUniqueID(LONG appid, const CHAR *&mem, LONG &bytes) const;
  // 	Bool GetUniqueIDIndex(LONG idx, LONG &id, const CHAR *&mem, LONG &bytes) const;
  // 	LONG GetUniqueIDCount() const;
  id = 0;
  appname = "Commandline Example";
}

Scene scene;

//-----------------------------------------------------------------------------
RootMaterial *AllocAlienRootMaterial()			{ return gNew RootMaterial; }
RootObject *AllocAlienRootObject()					{ return gNew RootObject; }
RootLayer *AllocAlienRootLayer()						{ return gNew RootLayer; }
RootRenderData *AllocAlienRootRenderData()	{ return gNew RootRenderData; }
RootViewPanel *AllocC4DRootViewPanel()			{ return gNew RootViewPanel; }
LayerObject *AllocAlienLayer()							{ return gNew LayerObject; }

//-----------------------------------------------------------------------------
Bool AlienMaterial::Execute()
{
  Print();
  return TRUE;
}

//-----------------------------------------------------------------------------
Bool AlienLayer::Execute()
{
  Print();
  return TRUE;
}

//-----------------------------------------------------------------------------
float* AddVector(float* f, const Vector& v)
{
  *f++ = v.x;
  *f++ = v.y;
  *f++ = v.z;
  return f;
}

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  Mesh* mesh = new Mesh(scene.meshes.size());

  // get object pointer
  PolygonObject* obj = (PolygonObject*)GetNode();

  // get point and polygon array pointer and counts
  const Vector* verts = obj->GetPointR();
  //u32 numVerts = obj->GetPointCount();

  const CPolygon* polys = obj->GetPolygonR();
  u32 numPolys = obj->GetPolygonCount();
  u32 numNGons = obj->GetNgonCount();

  // count # vertices. we create unique vertice per face.
  u32 numVerts = 0;
  for (u32 i = 0; i < numPolys; ++i)
  {
    numVerts += polys[i].c == polys[i].d ? 3 : 4;
  }

  // check if there are normals, tangents or UVW coordinates
  NormalTag* normals = obj->GetTag(Tnormal) ? (NormalTag*)obj->GetTag(Tnormal) : nullptr;

  const char* nameCStr = obj->GetName().GetCStringCopy();
  mesh->name = nameCStr;
  GeFree(nameCStr);

  // add verts
  mesh->verts.resize(numVerts*3);
  mesh->indices.reserve(3*numVerts);

  if (normals)
    mesh->normals.resize(numVerts*3);

  float* v = mesh->verts.data();
  float* n = mesh->normals.data();
  u32 vertOfs = 0;
  for (u32 i = 0; i < numPolys; ++i)
  {
    v = AddVector(v, verts[polys[i].a]);
    v = AddVector(v, verts[polys[i].b]);
    v = AddVector(v, verts[polys[i].c]);

    mesh->indices.push_back(vertOfs+0);
    mesh->indices.push_back(vertOfs+1);
    mesh->indices.push_back(vertOfs+2);

    bool quad = polys[i].c != polys[i].d;

    if (normals) {
      const NormalStruct& normal = normals->GetNormals(i);
      n = AddVector(n, normal.a);
      n = AddVector(n, normal.b);
      n = AddVector(n, normal.c);

      if (quad)
       n = AddVector(n, normal.d);
    }

    if (quad)
    {
      v = AddVector(v, verts[polys[i].d]);

      mesh->indices.push_back(vertOfs+0);
      mesh->indices.push_back(vertOfs+2);
      mesh->indices.push_back(vertOfs+3);
      vertOfs++;
    }

    vertOfs += 3;
  }

  scene.meshes.push_back(mesh);
  //Print();
  return TRUE;
}

//-----------------------------------------------------------------------------
NodeData *AllocAlienObjectData(LONG id, Bool &known)
{
  NodeData *m_data = NULL;
  known = TRUE;
  switch (id)
  {
    // supported element types
    case Opolygon:        m_data = gNew AlienPolygonObjectData; break;
    default:              known = FALSE; break;
  }

  return m_data;
}

// allocate the plugin tag elements data (only few tags have additional data which is stored in a NodeData)
NodeData *AllocAlienTagData(LONG id, Bool &known)
{
  return 0;
}


// create objects, material and layers for the new C4D scene file
Bool BaseDocument::CreateSceneToC4D(Bool selectedonly)
{
  return TRUE;
}

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  BaseDocument C4Ddoc;
  HyperFile C4Dfile;

  // old values: 
  // in: c:/temp/c4d/text1.c4d
  // out: d:/projects/boba/meshes/text1.boba

  if (argc < 2) {
    printf("No input file specified\n");
    return 1;
  }

  string name = argv[1];
  string outName(name + string(".boba"));

  // create output file
  if (argc > 2) {
    outName = argv[2];
  } else {
    if (const char* dot = strchr(name.c_str(), '.')) {
      int len = dot - name.c_str();
      outName = name.substr(0, len) + ".boba";
    }
  }

  if (!C4Dfile.Open(DOC_IDENT, name.c_str(), FILEOPEN_READ))
    return 1;

  if (!C4Ddoc.ReadObject(&C4Dfile, TRUE))
    return 1;

  C4Dfile.Close();
  C4Ddoc.CreateSceneFromC4D();
  scene.Save(outName.c_str());

  return 0;
}
