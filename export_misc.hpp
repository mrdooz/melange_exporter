#pragma once
#include "exporter.hpp"

void CollectionAnimationTracks(melange::BaseList2D* bl, vector<exporter::Track>* tracks);
void CollectMaterials(melange::AlienBaseDocument* c4dDoc);
void ExportSpline(melange::BaseObject* obj);

namespace melange
{
  //-----------------------------------------------------------------------------
  class AlienPrimitiveObjectData : public NodeData
  {
    INSTANCEOF(AlienPrimitiveObjectData, NodeData)
  public:
    virtual Bool Execute();
  };

  //-----------------------------------------------------------------------------
  class AlienNullObjectData : public NodeData
  {
    INSTANCEOF(AlienNullObjectData, NodeData)
  public:
    virtual Bool Execute();
  };


}
