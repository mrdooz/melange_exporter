#include "export_light.hpp"
#include "exporter_utils.hpp"
#include "exporter.hpp"
#include "export_misc.hpp"

//-----------------------------------------------------------------------------
bool melange::AlienLightObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  const string name = CopyString(baseObj->GetName());

  int lightType = GetInt32Param(baseObj, LIGHT_TYPE);

  if (lightType == LIGHT_TYPE_OMNI)
  {
  }
  else if (lightType == LIGHT_TYPE_DISTANT)
  {
  }
  else if (lightType == LIGHT_TYPE_SPOT)
  {
  }
  else
  {
    LOG(1, "Unsupported light type: %s\n", name.c_str());
    return true;
  }

  exporter::Light* light = new exporter::Light(baseObj);
  CollectionAnimationTracks(baseObj, &light->animTracks);
#if WITH_XFORM_MTX
  CopyMatrix(baseObj->GetMl(), light->mtxLocal);
  CopyMatrix(baseObj->GetMg(), light->mtxGlobal);
#endif

  CopyTransform(baseObj->GetMl(), &light->xformLocal);
  CopyTransform(baseObj->GetMg(), &light->xformGlobal);

  light->type = lightType;
  light->color = GetVectorParam<exporter::Color>(baseObj, LIGHT_COLOR);
  light->intensity = GetFloatParam(baseObj, LIGHT_BRIGHTNESS);

  light->falloffType = GetInt32Param(baseObj, LIGHT_DETAILS_FALLOFF);
  // LIGHT_DETAILS_FALLOFF_LINEAR = 8,
  if (light->falloffType == LIGHT_DETAILS_FALLOFF_LINEAR)
  {
    light->falloffRadius = GetFloatParam(baseObj, LIGHT_DETAILS_OUTERDISTANCE);
  }

  if (lightType == LIGHT_TYPE_DISTANT)
  {
  }
  else if (lightType == LIGHT_TYPE_SPOT)
  {
    light->outerAngle = GetFloatParam(baseObj, LIGHT_DETAILS_OUTERANGLE);
  }

  g_scene.lights.push_back(light);

  return true;
}
