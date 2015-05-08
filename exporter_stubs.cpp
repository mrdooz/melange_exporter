#include "exporter_stubs.hpp"

using namespace melange;
using namespace std;

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
