// Mostly contains stuff that's required by melange, and is kinda boilerplate

#pragma once
#include <c4d_file.h>
#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace melange
{
  // memory allocation functions inside _melange_ namespace (if you have your own memory management you can overload these functions)
  // alloc memory no clear
  void *MemAllocNC(Int size);

  // alloc memory set to 0
  void *MemAlloc(Int size);

  // realloc existing memory
  void *MemRealloc(void* orimem, Int size);

  // free memory and set pointer to null
  void MemFree(void *&mem);

  //-----------------------------------------------------------------------------
  class AlienMaterial : public Material
  {
  INSTANCEOF(AlienMaterial, Material)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  class AlienLayer : public LayerObject
  {
  INSTANCEOF(AlienLayer, LayerObject)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  class AlienPolygonObjectData : public PolygonObjectData
  {
  INSTANCEOF(AlienPolygonObjectData, PolygonObjectData)
  public:
    virtual Bool Execute();
  };

  //-----------------------------------------------------------------------------
  class AlienBaseDocument : public BaseDocument
  {
  INSTANCEOF(AlienBaseDocument, BaseDocument)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  class AlienPrimitiveObjectData : public NodeData
  {
  INSTANCEOF(AlienPrimitiveObjectData, NodeData)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  class AlienCameraObjectData : public CameraObjectData
  {
  INSTANCEOF(AlienCameraObjectData, CameraObjectData)
  public:
    AlienCameraObjectData() : CameraObjectData() {}
    virtual Bool Execute();
  };

  //-----------------------------------------------------------------------------
  class AlienNullObjectData : public NodeData
  {
  INSTANCEOF(AlienNullObjectData, NodeData)
  public:
    virtual Bool Execute();
  };

  //-----------------------------------------------------------------------------
  class AlienLightObjectData : public LightObjectData
  {
    INSTANCEOF(AlienLightObjectData, LightObjectData)
  public:
    virtual Bool Execute();
  };

}

// overload this function and fill in your own unique data
void GetWriterInfo(melange::Int32 &id, melange::String &appname);
