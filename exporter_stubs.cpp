// This is mostly a bunch of old crap that I can probably delete..

#include "exporter_stubs.hpp"

using namespace melange;

//-----------------------------------------------------------------------------
Bool AlienPolygonObjectData::Execute()
{
  return true;
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


//-----------------------------------------------------------------------------
NodeData *AllocAlienObjectData(Int32 id, Bool &known)
{
  NodeData *m_data = NULL;
  switch (id)
  {
    // supported element types
    case Opolygon:  m_data = NewObj(AlienPolygonObjectData); break;
    case Ocamera:   m_data = NewObj(AlienCameraObjectData); break;
    case Osphere:   m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocube:     m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Oplane:    m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocone:     m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Otorus:    m_data = NewObj(AlienPrimitiveObjectData, id); break;
    case Ocylinder: m_data = NewObj(AlienPrimitiveObjectData, id); break;
  }

  known = !!m_data;
  return m_data;
}

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
Bool AlienCameraObjectData::Execute()
{
  return true;
}

//-----------------------------------------------------------------------------
Bool AlienPrimitiveObjectData::Execute()
{
  return true;
  // decide the object type
  if (type_id == Ocube)
  {
    // get the size of the cube
    GeData cubeData;
    GetNode()->GetParameter(PRIM_CUBE_LEN, cubeData);
    Vector xyzLength = cubeData.GetVector();
    // create your own parametric cube with length X, Y and Z
    // ...
    // returning TRUE we will NOT get another call of PolygonObjectData Execute()
    // HACK! returning false means the object wasn't processed
    return false;
  }
  // return FALSE to get a simpler geometric description of objects like Osphere, Ocone, ...
  // the Execute() of AlienPolygonObjectData will be called
  return false;
}
