#include "exporter.hpp"
#include "deferred_writer.hpp"
#include "boba_scene.hpp"

using namespace boba;

void SaveMaterial(const Material* material, DeferredWriter& writer);
void SaveMesh(const Mesh* mesh, DeferredWriter& writer);
void SaveCamera(const Camera* camera, DeferredWriter& writer);
void SaveLight(const Light* light, DeferredWriter& writer);
void SaveNullObject(const NullObject* nullObject, DeferredWriter& writer);

//------------------------------------------------------------------------------
bool SaveScene(const Scene& scene, const Options& options)
{
  boba::DeferredWriter writer;
  if (!writer.Open(options.outputFilename.c_str()))
    return false;

  protocol::SceneBlob header;
  memset(&header, 0, sizeof(header));
  header.id[0] = 'b';
  header.id[1] = 'o';
  header.id[2] = 'b';
  header.id[3] = 'a';

  // dummy write the header
  writer.Write(header);

  header.numNullObjects = (u32)scene.nullObjects.size();
  header.nullObjectDataStart = header.numNullObjects ? (u32)writer.GetFilePos() : 0;
  for (NullObject* obj : scene.nullObjects)
    SaveNullObject(obj, writer);

  header.numMeshes = (u32)scene.meshes.size();
  header.meshDataStart = header.numMeshes ? (u32)writer.GetFilePos() : 0;
  for (Mesh* mesh : scene.meshes)
    SaveMesh(mesh, writer);

  header.numLights = (u32)scene.lights.size();
  header.lightDataStart = header.numLights ? (u32)writer.GetFilePos() : 0;
  for (const Light* light : scene.lights)
    SaveLight(light, writer);

  header.numCameras = (u32)scene.cameras.size();
  header.cameraDataStart = header.numCameras ? (u32)writer.GetFilePos() : 0;
  for (const Camera* camera : scene.cameras)
    SaveCamera(camera, writer);

  header.numMaterials = (u32)scene.materials.size();
  header.materialDataStart = header.numMaterials ? (u32)writer.GetFilePos() : 0;
  for (const Material* material : scene.materials)
    SaveMaterial(material, writer);

  header.fixupOffset = (u32)writer.GetFilePos();
  writer.WriteDeferredData();

  // write back the correct header
  writer.SetFilePos(0);
  writer.Write(header);

  return true;
}

//------------------------------------------------------------------------------
void SaveMaterial(const Material* material, DeferredWriter& writer)
{
  writer.StartBlockMarker();

  writer.AddDeferredString(material->name);
  writer.Write(material->id);
  writer.Write(material->flags);

  u32 colorFixup = (material->flags & Material::FLAG_COLOR) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  u32 luminanceFixup = (material->flags & Material::FLAG_LUMINANCE) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  u32 reflectionFixup = (material->flags & Material::FLAG_REFLECTION) ? writer.CreateFixup() : ~0u;
  writer.WritePtr(0);

  if (material->flags & Material::FLAG_COLOR)
  {
    writer.InsertFixup(colorFixup);
    writer.Write(material->color.color);
    writer.AddDeferredString(material->color.texture);
    writer.Write(material->color.brightness);
  }

  if (material->flags & Material::FLAG_LUMINANCE)
  {
    writer.InsertFixup(luminanceFixup);
    writer.Write(material->luminance.color);
    writer.AddDeferredString(material->luminance.texture);
    writer.Write(material->luminance.brightness);
  }

  if (material->flags & Material::FLAG_REFLECTION)
  {
    writer.InsertFixup(reflectionFixup);
    writer.Write(material->reflection.color);
    writer.AddDeferredString(material->reflection.texture);
    writer.Write(material->reflection.brightness);
  }

  writer.EndBlockMarker();
}

//------------------------------------------------------------------------------
void SaveBase(const BaseObject* base, DeferredWriter& writer)
{
  writer.AddDeferredString(base->name);
  writer.Write(base->id);
  writer.Write(base->parent ? base->parent->id : (u32)~0u);
  writer.Write(base->mtxLocal);
  writer.Write(base->mtxGlobal);
}

//------------------------------------------------------------------------------
void SaveMesh(const Mesh* mesh, DeferredWriter& writer)
{
  SaveBase(mesh, writer);

  // Note, divide by 3 here to write # verts, and not # floats
  writer.Write((u32)mesh->verts.size() / 3);
  writer.Write((u32)mesh->indices.size());

  // write the material groups
  writer.Write((u32)mesh->materialGroups.size());
  writer.AddDeferredVector(mesh->materialGroups);

  writer.AddDeferredVector(mesh->verts);
  writer.AddDeferredVector(mesh->normals);
  writer.AddDeferredVector(mesh->uv);

  writer.AddDeferredVector(mesh->indices);

  // save bounding box
  writer.Write(mesh->boundingSphere);
}

//------------------------------------------------------------------------------
void SaveCamera(const Camera* camera, DeferredWriter& writer)
{
  SaveBase(camera, writer);

  writer.Write(camera->verticalFov);
  writer.Write(camera->nearPlane);
  writer.Write(camera->farPlane);
}

//------------------------------------------------------------------------------
void SaveLight(const Light* light, DeferredWriter& writer)
{
  SaveBase(light, writer);
  
  writer.Write(light->type);
  writer.Write(light->color);
  writer.Write(light->intensity);
}

//------------------------------------------------------------------------------
void SaveNullObject(const NullObject* nullObject, DeferredWriter& writer)
{
  SaveBase(nullObject, writer);
}
