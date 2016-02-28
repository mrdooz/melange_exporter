#include "boba_scene_format.hpp"
#include "deferred_writer.hpp"
#include "exporter.hpp"
#include "save_scene.hpp"

#include "compress/forsythtriangleorderoptimizer.h"
#include "compress/indexbuffercompression.h"
#include "compress/indexbufferdecompression.h"
#include "compress/oct.h"

//------------------------------------------------------------------------------
struct ScopedStats
{
  ScopedStats(const DeferredWriter& writer, int* val) : writer(writer), val(val)
  {
    *val = writer.GetFilePos();
  }
  ~ScopedStats() { *val = -(*val - writer.GetFilePos()); }
  const DeferredWriter& writer;
  int* val;
};

//------------------------------------------------------------------------------
bool SaveScene(const Scene& scene, const Options& options, SceneStats* stats)
{
  DeferredWriter writer;
  if (!writer.Open(options.outputFilename.c_str()))
    return false;

  protocol::SceneBlob header;
  memset(&header, 0, sizeof(header));
  header.id[0] = 'b';
  header.id[1] = 'o';
  header.id[2] = 'b';
  header.id[3] = 'a';

  header.flags = 0;
  header.version = BOBA_PROTOCOL_VERSION;

  // dummy write the header
  writer.Write(header);

  {
    ScopedStats s(writer, &stats->nullObjectSize);
    header.numNullObjects = (u32)scene.nullObjects.size();
    header.nullObjectDataStart = header.numNullObjects ? (u32)writer.GetFilePos() : 0;
    for (NullObject* obj : scene.nullObjects)
    {
      SaveNullObject(obj, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->meshSize);
    header.numMeshes = (u32)scene.meshes.size();
    header.meshDataStart = header.numMeshes ? (u32)writer.GetFilePos() : 0;
    for (Mesh* mesh : scene.meshes)
    {
      SaveMesh(mesh, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->lightSize);
    header.numLights = (u32)scene.lights.size();
    header.lightDataStart = header.numLights ? (u32)writer.GetFilePos() : 0;
    for (const Light* light : scene.lights)
    {
      SaveLight(light, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->cameraSize);
    header.numCameras = (u32)scene.cameras.size();
    header.cameraDataStart = header.numCameras ? (u32)writer.GetFilePos() : 0;
    for (const Camera* camera : scene.cameras)
    {
      SaveCamera(camera, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->materialSize);
    header.numMaterials = (u32)scene.materials.size();
    header.materialDataStart = header.numMaterials ? (u32)writer.GetFilePos() : 0;
    for (const Material* material : scene.materials)
    {
      SaveMaterial(material, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->splineSize);
    header.numSplines = (u32)scene.splines.size();
    header.splineDataStart = header.numSplines ? (u32)writer.GetFilePos() : 0;
    for (const Spline* spline : scene.splines)
    {
      SaveSpline(spline, options, writer);
    }
  }

  {
    ScopedStats s(writer, &stats->dataSize);
    header.fixupOffset = (u32)writer.GetFilePos();
    writer.WriteDeferredData();
  }

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

  u32 colorFixup =
    (material->flags & Material::FLAG_COLOR) ? writer.CreateFixup() : protocol::INVALID_OBJECT_ID;

  u32 luminanceFixup = (material->flags & Material::FLAG_LUMINANCE) ? writer.CreateFixup()
    : protocol::INVALID_OBJECT_ID;

  u32 reflectionFixup = (material->flags & Material::FLAG_REFLECTION) ? writer.CreateFixup()
    : protocol::INVALID_OBJECT_ID;

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
  writer.Write(base->parent ? base->parent->id : protocol::INVALID_OBJECT_ID);
#if BOBA_PROTOCOL_VERSION < 4
  writer.Write(base->mtxLocal);
  writer.Write(base->mtxGlobal);
#endif
#if BOBA_PROTOCOL_VERSION >= 4
  writer.Write(base->xformLocal);
  writer.Write(base->xformGlobal);
#endif
}

struct Nibbler
{
  Nibbler()
  {
    bytes.push_back(0);
  }

  void AddNibble(u8 nibble)
  {
    if (phase)
    {
      bytes.back() |= (nibble << 4);
      bytes.push_back(0);
    }
    else
    {
      bytes.back() |= nibble;
    }

    phase ^= 1;
  }

  bool HasNibble(u8 nibble)
  {
    for (u8 b : bytes)
    {
      if ((b & 0xf) == nibble || ((b >> 4) & 0xf) == nibble)
        return true;
    }

    return false;
  }

  int phase = 0;
  vector<u8> bytes;
};

struct VertexFormat
{
  void Add(protocol::VertexFormat2 fmt, protocol::VertexFlags2 flags)
  {
    bytes.push_back(((u32)flags << 16) + (u32)fmt);
  }

  void End()
  {
    bytes.push_back(0);
  }

  bool HasFormat(protocol::VertexFormat2 fmt)
  {
    for (u32 b : bytes)
    {
      if ((b & 0xffff) == fmt)
        return true;
    }

    return false;
  }

  vector<u32> bytes;
};

//------------------------------------------------------------------------------
void SaveCompressedVertices(Mesh* mesh, const Options& options, DeferredWriter& writer)
{
  {
    // normals
    size_t numNormals = mesh->normals.size() / 3;
    vector<s16> compressedNormals(numNormals * 2);

    auto Normalize = [](float* v) {
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

    {
      // texture coords
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

    {
      // indices
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
}

//------------------------------------------------------------------------------
void OptimizeFaces(Mesh* mesh)
{
  vector<int> optimizedIndices(mesh->indices.size());
  Forsyth::OptimizeFaces((const u32*)mesh->indices.data(),
    (u32)mesh->indices.size(),
    (u32)mesh->verts.size() / 3,
    (u32*)optimizedIndices.data(),
    32);
  mesh->indices.swap(optimizedIndices);
}

//------------------------------------------------------------------------------
void SaveMesh(Mesh* mesh, const Options& options, DeferredWriter& writer)
{
  bool useCompression = false;
  bool optimizeFaces = false;

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

  vector<float> vertexData;
  VertexFormat fmt;
  if (mesh->verts.size())
  {
    fmt.Add(protocol::VFORMAT2_POS, protocol::VFLAG2_NONE);
    copy(mesh->verts.begin(), mesh->verts.end(), back_inserter(vertexData));
  }

  if (mesh->normals.size())
  {
    fmt.Add(protocol::VFORMAT2_NORMAL, protocol::VFLAG2_NONE);
    copy(mesh->normals.begin(), mesh->normals.end(), back_inserter(vertexData));
  }

  // Don't save UVs if using the default material
  if (mesh->uv.size() && mesh->materialGroups.size() == 1
    && mesh->materialGroups[0].materialId == DEFAULT_MATERIAL)
  {
    fmt.Add(protocol::VFORMAT2_TEX2, protocol::VFLAG2_NONE);
    copy(mesh->uv.begin(), mesh->uv.end(), back_inserter(vertexData));
  }

  fmt.End();

  writer.AddDeferredVector(fmt.bytes);

  if (useCompression)
  {
    SaveCompressedVertices(mesh, options, writer);
  }
  else
  {
    writer.AddDeferredVector(vertexData);

    if (optimizeFaces)
    {
      OptimizeFaces(mesh);
    }

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
  writer.Write(camera->targetObj->id);
}

//------------------------------------------------------------------------------
void SaveLight(const Light* light, const Options& options, DeferredWriter& writer)
{
  SaveBase(light, options, writer);

  unordered_map<int, protocol::LightType> typeLookup = {
    { melange::LIGHT_TYPE_OMNI, protocol::LightType::Point },
    { melange::LIGHT_TYPE_DISTANT, protocol::LightType::Directional },
    { melange::LIGHT_TYPE_SPOT, protocol::LightType::Spot } };

  unordered_map<int, protocol::FalloffType> falloffLookup = {
    { melange::LIGHT_DETAILS_FALLOFF_NONE, protocol::FalloffType::None },
    { melange::LIGHT_DETAILS_FALLOFF_LINEAR, protocol::FalloffType::Linear } };

  writer.Write(typeLookup[light->type]);
  writer.Write(light->color);
  writer.Write(light->intensity);

  writer.Write(falloffLookup[light->falloffType]);
  writer.Write(light->falloffRadius);
  writer.Write(light->outerAngle);
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
