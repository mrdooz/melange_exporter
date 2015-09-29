#include "exporter.hpp"
#include "deferred_writer.hpp"
#include "boba_scene_format.hpp"

#include "compress/indexbuffercompression.h"
#include "compress/indexbufferdecompression.h"
#include "compress/forsythtriangleorderoptimizer.h"
#include "compress/oct.h"

using namespace boba;

void SaveMaterial(const Material* material, const Options& options, DeferredWriter& writer);
void SaveMesh(Mesh* mesh, const Options& options, DeferredWriter& writer);
void SaveCamera(const Camera* camera, const Options& options, DeferredWriter& writer);
void SaveLight(const Light* light, const Options& options, DeferredWriter& writer);
void SaveNullObject(const NullObject* nullObject, const Options& options, DeferredWriter& writer);
void SaveSpline(const Spline* spline, const Options& options, DeferredWriter& writer);

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

  header.flags = 0;
  header.version = 2;

  // dummy write the header
  writer.Write(header);

  header.numNullObjects = (u32)scene.nullObjects.size();
  header.nullObjectDataStart = header.numNullObjects ? (u32)writer.GetFilePos() : 0;
  for (NullObject* obj : scene.nullObjects)
  {
    SaveNullObject(obj, options, writer);
  }

  header.numMeshes = (u32)scene.meshes.size();
  header.meshDataStart = header.numMeshes ? (u32)writer.GetFilePos() : 0;
  for (Mesh* mesh : scene.meshes)
  {
    SaveMesh(mesh, options, writer);
  }

  header.numLights = (u32)scene.lights.size();
  header.lightDataStart = header.numLights ? (u32)writer.GetFilePos() : 0;
  for (const Light* light : scene.lights)
  {
    SaveLight(light, options, writer);
  }

  header.numCameras = (u32)scene.cameras.size();
  header.cameraDataStart = header.numCameras ? (u32)writer.GetFilePos() : 0;
  for (const Camera* camera : scene.cameras)
  {
    SaveCamera(camera, options, writer);
  }

  header.numMaterials = (u32)scene.materials.size();
  header.materialDataStart = header.numMaterials ? (u32)writer.GetFilePos() : 0;
  for (const Material* material : scene.materials)
  {
    SaveMaterial(material, options, writer);
  }

  header.numSplines = (u32)scene.splines.size();
  header.splineDataStart = header.numSplines ? (u32)writer.GetFilePos() : 0;
  for (const Spline* spline : scene.splines)
  {
    SaveSpline(spline, options, writer);
  }

  header.fixupOffset = (u32)writer.GetFilePos();
  writer.WriteDeferredData();

  // write back the correct header
  writer.SetFilePos(0);
  writer.Write(header);

  return true;
}

//------------------------------------------------------------------------------
void SaveMaterial(const Material* material, const Options& options, DeferredWriter& writer)
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
void SaveBase(const BaseObject* base, const Options& options, DeferredWriter& writer)
{
  writer.AddDeferredString(base->name);
  writer.Write(base->id);
  writer.Write(base->parent ? base->parent->id : (u32)~0u);
  writer.Write(base->mtxLocal);
  writer.Write(base->mtxGlobal);
}

//------------------------------------------------------------------------------
void SaveMesh(Mesh* mesh, const Options& options, DeferredWriter& writer)
{
  bool useCompression = false;

  // Compression stats:
  // org: 2,030,104 crystals_flat.boba
  // compressed indices: 1,712,177 crystals_flat.boba
  // compressed indices + compressed normals: 1,213,649 crystals_flat.boba
  SaveBase(mesh, options, writer);

  // Note, divide by 3 here to write # verts, and not # floats
  writer.Write((u32)mesh->verts.size() / 3);
  writer.Write((u32)mesh->indices.size());

  // write the material groups
  writer.Write((u32)mesh->materialGroups.size());
  writer.AddDeferredVector(mesh->materialGroups);
  writer.AddDeferredVector(mesh->verts);

  if (useCompression)
  {
    size_t numNormals = mesh->normals.size() / 3;
    vector<s16> compressedNormals(numNormals * 2);

    auto Normalize = [](float* v)
    {
      float x = v[0];
      float y = v[1];
      float z = v[2];
      float len = sqrt(x * x + y * y + z * z);
      v[0] /= len;
      v[1] /= len;
      v[2] /= len;
    };

    for (size_t i = 0; i < numNormals; ++i)
    {
      Snorm<snormSize> res[2];
      Normalize(mesh->normals.data() + i * 3);
      octPEncode(&mesh->normals[i * 3], res);
      compressedNormals[i * 2 + 0] = res[0].bits() & 0xffff;
      compressedNormals[i * 2 + 1] = res[1].bits() & 0xffff;
    }

    writer.AddDeferredVector(compressedNormals);
  }
  else
  {
    writer.AddDeferredVector(mesh->normals);
  }

  // Don't save UVs if using the default material
  if (mesh->materialGroups.size() == 1
      && mesh->materialGroups[0].materialId == boba::DEFAULT_MATERIAL)
  {
    writer.AddDeferredVector(vector<float>());
  }
  else
  {
    if (useCompression)
    {
      size_t numUvs = mesh->uv.size();
      vector<s16> compressedUV(numUvs);
      for (size_t i = 0; i < numUvs / 2; ++i)
      {
        Snorm<snormSize> u(mesh->uv[i * 2 + 0]);
        Snorm<snormSize> v(mesh->uv[i * 2 + 1]);
        compressedUV[i * 2 + 0] = u.bits() & 0xffff;
        compressedUV[i * 2 + 1] = v.bits() & 0xffff;
      }
      writer.AddDeferredVector(compressedUV);
    }
    else
    {
      writer.AddDeferredVector(mesh->uv);
    }
  }

  vector<int> optimizedIndices(mesh->indices.size());
  Forsyth::OptimizeFaces((const u32*)mesh->indices.data(),
      (u32)mesh->indices.size(),
      (u32)mesh->verts.size() / 3,
      (u32*)optimizedIndices.data(),
      32);
  mesh->indices.swap(optimizedIndices);

  if (useCompression)
  {
    WriteBitstream output;
    vector<u32> vertexRemap(mesh->verts.size() / 3);
    CompressIndexBuffer((const u32*)mesh->indices.data(),
        (u32)mesh->indices.size() / 3,
        vertexRemap.data(),
        (u32)vertexRemap.size(),
        IBCF_AUTO,
        output);
    // TODO: remap the vertices
    vector<u8> compressedIndices(output.ByteSize());
    writer.AddDeferredVector(compressedIndices);
  }
  else
  {
    writer.AddDeferredVector(mesh->indices);
  }

  writer.Write((int)mesh->selectedEdges.size());
  writer.AddDeferredVector(mesh->selectedEdges);

  // save bounding volume
  writer.Write(mesh->boundingSphere);
}

//------------------------------------------------------------------------------
void SaveCamera(const Camera* camera, const Options& options, DeferredWriter& writer)
{
  SaveBase(camera, options, writer);

  writer.Write(camera->verticalFov);
  writer.Write(camera->nearPlane);
  writer.Write(camera->farPlane);
}

//------------------------------------------------------------------------------
void SaveLight(const Light* light, const Options& options, DeferredWriter& writer)
{
  SaveBase(light, options, writer);

  writer.Write(light->type);
  writer.Write(light->color);
  writer.Write(light->intensity);
}

//------------------------------------------------------------------------------
void SaveSpline(const Spline* spline, const Options& options, DeferredWriter& writer)
{
  SaveBase(spline, options, writer);

  writer.Write(spline->type);
  writer.Write((int)spline->points.size() / 3);
  writer.AddDeferredVector(spline->points);
  writer.Write(spline->isClosed);
}

//------------------------------------------------------------------------------
void SaveNullObject(const NullObject* nullObject, const Options& options, DeferredWriter& writer)
{
  SaveBase(nullObject, options, writer);
}
