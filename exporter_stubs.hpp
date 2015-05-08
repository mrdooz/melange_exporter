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
}
