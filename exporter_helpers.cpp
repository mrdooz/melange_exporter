#include "exporter_helpers.hpp"

using namespace melange;

// overload this function and fill in your own unique data
void GetWriterInfo(Int32 &id, String &appname)
{
  // register your own pluginid once for your exporter and enter it here under id
  // this id must be used for your own unique ids
  // 	Bool AddUniqueID(LONG appid, const CHAR *const mem, LONG bytes);
  // 	Bool FindUniqueID(LONG appid, const CHAR *&mem, LONG &bytes) const;
  // 	Bool GetUniqueIDIndex(LONG idx, LONG &id, const CHAR *&mem, LONG &bytes) const;
  // 	LONG GetUniqueIDCount() const;
  id = 0;
  appname = "C4d -> boba converter";
}

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
    case Onull:     m_data = NewObj(AlienNullObjectData); break;
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


namespace melange
{
  // memory allocation functions inside _melange_ namespace (if you have your own memory management you can overload these functions)
  // alloc memory no clear
  void *MemAllocNC(Int size)
  {
    void *mem = malloc(size);
    return mem;
  }

  // alloc memory set to 0
  void *MemAlloc(Int size)
  {
    void *mem = MemAllocNC(size);
    memset(mem, 0, size);
    return mem;
  }

  // realloc existing memory
  void *MemRealloc(void* orimem, Int size)
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
