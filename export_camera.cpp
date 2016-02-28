#include "melange_helpers.hpp"
#include "export_camera.hpp"
#include "export_misc.hpp"
#include "exporter.hpp"
#include "exporter_utils.hpp"

static const float DEFAULT_NEAR_PLANE = 1.f;
static const float DEFAULT_FAR_PLANE = 1000.f;

//-----------------------------------------------------------------------------
bool melange::AlienCameraObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  const string name = CopyString(baseObj->GetName());

  // Check if the camera is a target camera
  BaseTag* targetTag = baseObj->GetTag(Ttargetexpression);
  if (!targetTag)
  {
    // Not a target camera, so require the parten object to be a null object
    BaseObject* parent = baseObj->GetUp();
    bool isNullParent = parent && parent->GetType() == OBJECT_NULL;
    if (!isNullParent)
    {
      LOG(1, "Camera's %s parent isn't a null object!\n", name.c_str());
      return false;
    }
  }

  int projectionType = GetInt32Param(baseObj, CAMERA_PROJECTION);
  if (projectionType != Pperspective)
  {
    LOG(2,
      "Skipping camera (%s) with unsupported projection type (%d)\n",
      name.c_str(),
      projectionType);
    return false;
  }

  exporter::Camera* camera = new exporter::Camera(baseObj);

  if (targetTag)
  {
    BaseObject* targetObj = targetTag->GetDataInstance()->GetObjectLink(TARGETEXPRESSIONTAG_LINK);
    g_deferredFunctions.push_back([=]() {
      camera->targetObj = g_scene.FindObject(targetObj);
      if (!camera->targetObj)
      {
        LOG(1, "Unable to find target object: %s", CopyString(targetObj->GetName()).c_str());
        return false;
      }
      return true;
    });
  }

  CollectionAnimationTracks(baseObj, &camera->animTracks);
#if WITH_XFORM_MTX
  CopyMatrix(baseObj->GetMl(), camera->mtxLocal);
  CopyMatrix(baseObj->GetMg(), camera->mtxGlobal);
#endif

  CopyTransform(baseObj->GetMl(), &camera->xformLocal);
  CopyTransform(baseObj->GetMg(), &camera->xformGlobal);

  camera->verticalFov = GetFloatParam(baseObj, CAMERAOBJECT_FOV_VERTICAL);
  camera->nearPlane =
    GetInt32Param(baseObj, CAMERAOBJECT_NEAR_CLIPPING_ENABLE)
    ? max(DEFAULT_NEAR_PLANE, GetFloatParam(baseObj, CAMERAOBJECT_NEAR_CLIPPING))
    : DEFAULT_NEAR_PLANE;
  camera->farPlane = GetInt32Param(baseObj, CAMERAOBJECT_FAR_CLIPPING_ENABLE)
    ? GetFloatParam(baseObj, CAMERAOBJECT_FAR_CLIPPING)
    : DEFAULT_FAR_PLANE;

  if (camera->valid)
  {
    g_scene.cameras.push_back(camera);
  }
  else
  {
    delete camera;
  }
  return true;
}
