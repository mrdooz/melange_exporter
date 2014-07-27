#pragma once

#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>

namespace _melange_
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


}