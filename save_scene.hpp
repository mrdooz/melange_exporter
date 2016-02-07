#pragma once

namespace boba
{
  bool SaveScene(const Scene& scene, const Options& options, SceneStats* stats);
  void SaveMaterial(const Material* material, const Options& options, DeferredWriter& writer);
  void SaveMesh(Mesh* mesh, const Options& options, DeferredWriter& writer);
  void SaveCamera(const Camera* camera, const Options& options, DeferredWriter& writer);
  void SaveLight(const Light* light, const Options& options, DeferredWriter& writer);
  void SaveNullObject(const NullObject* nullObject, const Options& options, DeferredWriter& writer);
  void SaveSpline(const Spline* spline, const Options& options, DeferredWriter& writer);
}
