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
Bool AlienPolygonObjectData::Execute()
{
  Mesh* mesh = new Mesh(scene.meshes.size());

  // get object pointer
  PolygonObject* obj = (PolygonObject*)GetNode();

  // get point and polygon array pointer and counts
  const Vector* verts = obj->GetPointR();
  u32 numVerts = obj->GetPointCount();

  const CPolygon* polys = obj->GetPolygonR();
  u32 numPolys = obj->GetPolygonCount();

  u32 numNGons = obj->GetNgonCount();
  if (numNGons > 0)
  {
    int a = 10;
  }

  // currently not exporting normals
  //const SVector* normals = obj->CreatePhongNormals();

  const char* nameCStr = obj->GetName().GetCStringCopy();
  mesh->name = nameCStr;
  GeFree(nameCStr);

  // add verts
  mesh->verts.resize(numVerts*3);
  for (u32 i = 0; i < numVerts; ++i) {
    mesh->verts[i*3+0] = verts[i].x;
    mesh->verts[i*3+1] = verts[i].y;
    mesh->verts[i*3+2] = verts[i].z;
  }

  // add faces
  // tris are identified by the fact that their last two indices are the same

  mesh->indices.reserve(3*2*numPolys);
  for (u32 i = 0; i < numPolys; ++i) {
    u32 a = polys[i].a;
    u32 b = polys[i].b;
    u32 c = polys[i].c;
    u32 d = polys[i].d;

    mesh->indices.push_back(a);
    mesh->indices.push_back(c);
    mesh->indices.push_back(b);

    if (c != d)
    {
      mesh->indices.push_back(a);
      mesh->indices.push_back(d);
      mesh->indices.push_back(c);
    }
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

  const char* name = "c:/temp/c4d/text1.c4d";
  if (!C4Dfile.Open(DOC_IDENT, name, FILEOPEN_READ))
    return 1;

  if (!C4Ddoc.ReadObject(&C4Dfile, TRUE))
    return 1;

  C4Dfile.Close();
  C4Ddoc.CreateSceneFromC4D();

  string outName(name + string(".boba"));
  outName = "d:/projects/boba/meshes/text1.boba";
  scene.Save(outName.c_str());

  return 0;
}
