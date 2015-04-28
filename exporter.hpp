#pragma once

#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>

namespace melange
{
  class AlienMaterial : public Material
  {
    INSTANCEOF(AlienMaterial, Material)

  public:

    LONG id;

    AlienMaterial() : Material() { id = 0; }
    virtual Bool Execute();
    void Print();
  };


  //self defined layer with own functions and members
  class AlienLayer : public LayerObject
  {
    INSTANCEOF(AlienLayer, LayerObject)

  public:

    LONG id;

    AlienLayer() : LayerObject() { }
    virtual Bool Execute();
    void Print();
  };

  //self defined polygon object data with own functions and members
  class AlienPolygonObjectData : public PolygonObjectData
  {
    INSTANCEOF(AlienPolygonObjectData, PolygonObjectData)

  public:
    LONG layid;
    LONG matid;

    virtual Bool Execute();
    void Print();
  };

  // this self defined alien primitive object data will “understand” a Cube as parametric Cube
  // all other objects are unknown and we will get a polygonal description for them
  class AlienPrimitiveObjectData : public NodeData
  {
    INSTANCEOF(AlienPrimitiveObjectData, NodeData)
    LONG type_id;
  public:
    AlienPrimitiveObjectData(LONG id) : type_id(id) {}
    virtual Bool Execute();
  };
}