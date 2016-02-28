#pragma once

namespace melange
{
  //-----------------------------------------------------------------------------
  class AlienCameraObjectData : public CameraObjectData
  {
    INSTANCEOF(AlienCameraObjectData, CameraObjectData)
  public:
    AlienCameraObjectData() : CameraObjectData() {}
    virtual Bool Execute();
  };
}
