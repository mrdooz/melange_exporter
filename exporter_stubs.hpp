#include <c4d_file.h>
#include <c4d_ccurve.h>
#include <c4d_ctrack.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace melange
{
  //-----------------------------------------------------------------------------
  class AlienMaterial : public Material
  {
    INSTANCEOF(AlienMaterial, Material)

  public:

    int id;

    AlienMaterial() : Material() { id = 0; }
    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  class AlienCameraObjectData : public CameraObjectData
  {
    INSTANCEOF(AlienCameraObjectData, CameraObjectData)

  public:

    AlienCameraObjectData() : CameraObjectData() { }
    virtual Bool Execute();
  };

  //-----------------------------------------------------------------------------
  //self defined layer with own functions and members
  class AlienLayer : public LayerObject
  {
    INSTANCEOF(AlienLayer, LayerObject)

  public:

    int id;

    AlienLayer() : LayerObject() { }
    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  //self defined polygon object data with own functions and members
  class AlienPolygonObjectData : public PolygonObjectData
  {
    INSTANCEOF(AlienPolygonObjectData, PolygonObjectData)

  public:
    int layid;
    int matid;

    virtual Bool Execute();
    void Print();
  };

  //-----------------------------------------------------------------------------
  class AlienBaseDocument : public BaseDocument
  {
    INSTANCEOF(AlienBaseDocument, BaseDocument)
  public:
    virtual Bool Execute() { return true; }
  };

  //-----------------------------------------------------------------------------
  // this self defined alien primitive object data will �understand� a Cube as parametric Cube
  // all other objects are unknown and we will get a polygonal description for them
  class AlienPrimitiveObjectData : public NodeData
  {
    INSTANCEOF(AlienPrimitiveObjectData, NodeData)
    int type_id;
  public:
    AlienPrimitiveObjectData(int id) : type_id(id) {}
    virtual Bool Execute();
  };
}