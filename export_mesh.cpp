#include "export_mesh.hpp"
#include "exporter.hpp"
#include "export_misc.hpp"
#include "exporter_utils.hpp"

//-----------------------------------------------------------------------------
void AddIndices(vector<int>* indices, int a, int b, int c)
{
  indices->push_back(a);
  indices->push_back(b);
  indices->push_back(c);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddVector2(vector<float>* out, const T& v, bool flipY)
{
  out->push_back(v.x);
  out->push_back(flipY ? 1 - v.y : v.y);
};

//-----------------------------------------------------------------------------
template <typename T>
void AddVector3(vector<float>* out, const T& v)
{
  out->push_back(v.x);
  out->push_back(v.y);
  out->push_back(v.z);
};

//-----------------------------------------------------------------------------
template <typename T>
void Add3Vector2(vector<float>* out, const T& a, const T& b, const T& c, bool flipY)
{
  AddVector2(out, a, flipY);
  AddVector2(out, b, flipY);
  AddVector2(out, c, flipY);
};

//-----------------------------------------------------------------------------
template <typename T>
void Add3Vector3(vector<float>* out, const T& a, const T& b, const T& c)
{
  AddVector3(out, a);
  AddVector3(out, b);
  AddVector3(out, c);
};

//-----------------------------------------------------------------------------
melange::Vector CalcNormal(
  const melange::Vector& a, const melange::Vector& b, const melange::Vector& c)
{
  melange::Vector e0 = (b - a);
  e0.Normalize();

  melange::Vector e1 = (c - a);
  e1.Normalize();

  return Cross(e0, e1);
}

//-----------------------------------------------------------------------------
template <typename T>
inline bool IsQuad(const T& p)
{
  return p.c != p.d;
}

//-----------------------------------------------------------------------------
static void CollectVertices(melange::PolygonObject* polyObj, exporter::Mesh* mesh)
{
  // get point and polygon array pointer and counts
  const melange::Vector* verts = polyObj->GetPointR();
  const melange::CPolygon* polysOrg = polyObj->GetPolygonR();

  int vertexCount = polyObj->GetPointCount();
  if (!vertexCount)
    return;

  // calc bounding sphere (get center and max radius)
  melange::Vector center(verts[0]);
  for (int i = 1; i < vertexCount; ++i)
  {
    center += verts[i];
  }
  center /= vertexCount;

  float radius = (center - verts[1]).GetSquaredLength();
  for (int i = 2; i < vertexCount; ++i)
  {
    float cand = (center - verts[i]).GetSquaredLength();
    radius = max(radius, cand);
  }

  mesh->boundingSphere.center = center;
  mesh->boundingSphere.radius = sqrtf(radius);

  // Loop over all the materials, and add the polygons in the order they appear per material
  struct Polygon
  {
    int a, b, c, d;
  };

  vector<Polygon> polys;
  polys.reserve(polyObj->GetPolygonCount());

  int numPolys = 0;
  int idx = 0;
  for (const pair<u32, vector<int>>& kv : mesh->polysByMaterial)
  {
    exporter::Mesh::MaterialGroup mg;
    mg.materialId = kv.first;
    mg.startIndex = idx;
    numPolys += (int)kv.second.size();

    for (int i : kv.second)
    {
      polys.push_back({ polysOrg[i].a, polysOrg[i].b, polysOrg[i].c, polysOrg[i].d });

      // increment index counter. if the polygon is a quad, then double the increment
      if (polysOrg[i].c == polysOrg[i].d)
        idx += 3;
      else
        idx += 2 * 3;
    }
    mg.numIndices = idx - mg.startIndex;
    mesh->materialGroups.push_back(mg);
  }

  u32 numVerts = 0;
  for (int i = 0; i < numPolys; ++i)
  {
    numVerts += IsQuad(polys[i]) ? 4 : 3;
  }

  // Check what kind of normals exist
  bool hasNormalsTag = !!polyObj->GetTag(Tnormal);
  bool hasPhongTag = !!polyObj->GetTag(Tphong);
  bool hasNormals = hasNormalsTag || hasPhongTag;

  melange::Vector32* phongNormals = hasPhongTag ? polyObj->CreatePhongNormals() : nullptr;
  melange::NormalTag* normals =
    hasNormalsTag ? (melange::NormalTag*)polyObj->GetTag(Tnormal) : nullptr;
  melange::UVWTag* uvs = (melange::UVWTag*)polyObj->GetTag(Tuvw);
  melange::ConstUVWHandle uvHandle = uvs ? uvs->GetDataAddressR() : nullptr;

  // add verts
  mesh->verts.reserve(numVerts * 3);
  mesh->normals.reserve(numVerts * 3);
  mesh->indices.reserve(numVerts * 3);
  mesh->uv.reserve(numVerts * 3);

  melange::ConstNormalHandle normalHandle = normals ? normals->GetDataAddressR() : nullptr;

  u32 vertOfs = 0;

  for (int i = 0; i < numPolys; ++i)
  {
    int idx0 = polys[i].a;
    int idx1 = polys[i].b;
    int idx2 = polys[i].c;
    int idx3 = polys[i].d;
    bool isQuad = IsQuad(polys[i]);

    // face 0, 1, 2
    Add3Vector3(&mesh->verts, verts[idx0], verts[idx1], verts[idx2]);
    AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 1, vertOfs + 2);

    if (isQuad)
    {
      // face 0, 2, 3
      AddVector3(&mesh->verts, verts[idx3]);
      AddIndices(&mesh->indices, vertOfs + 0, vertOfs + 2, vertOfs + 3);
      vertOfs += 4;
    }
    else
    {
      vertOfs += 3;
    }

    if (hasNormals)
    {
      if (hasNormalsTag)
      {
        melange::NormalStruct normal;
        normals->Get(normalHandle, i, normal);
        Add3Vector3(&mesh->normals, normal.a, normal.b, normal.c);

        if (isQuad)
        {
          AddVector3(&mesh->normals, normal.d);
        }
      }
      else if (hasPhongTag)
      {
        Add3Vector3(&mesh->normals,
          phongNormals[i * 4 + 0],
          phongNormals[i * 4 + 1],
          phongNormals[i * 4 + 2]);

        if (isQuad)
        {
          AddVector3(&mesh->normals, phongNormals[i * 4 + 3]);
        }
      }
    }
    else
    {
      // no normals, so generate polygon normals and use them for all verts
      melange::Vector normal = CalcNormal(verts[idx0], verts[idx1], verts[idx2]);
      Add3Vector3(&mesh->normals, normal, normal, normal);
      if (isQuad)
      {
        AddVector3(&mesh->normals, normal);
      }
    }

    if (uvHandle)
    {
      melange::UVWStruct s;
      melange::UVWTag::Get(uvHandle, i, s);

      Add3Vector2(&mesh->uv, s.a, s.b, s.c, true);
      if (isQuad)
      {
        // face 0, 2, 3
        AddVector2(&mesh->uv, s.d, true);
      }
    }
  }

  if (phongNormals)
    melange::_MemFree((void**)&phongNormals);
}

//-----------------------------------------------------------------------------
static void CollectMeshMaterials(melange::PolygonObject* obj, exporter::Mesh* mesh)
{
  unordered_set<u32> selectedPolys;

  u32 prevMaterial = ~0u;
  u32 firstMaterial = ~0u;

  // For each material found, check if the following tag is a selection tag, in which
  // case record which polys belong to it
  for (melange::BaseTag* btag = obj->GetFirstTag(); btag; btag = btag->GetNext())
  {
    // texture tag
    if (btag->GetType() == Ttexture)
    {
      melange::GeData data;
      melange::AlienMaterial* mat = btag->GetParameter(melange::TEXTURETAG_MATERIAL, data)
        ? (melange::AlienMaterial*)data.GetLink()
        : NULL;
      if (!mat)
        continue;

      auto materialIt = find_if(
        RANGE(g_scene.materials), [mat](const exporter::Material* m) { return m->mat == mat; });
      if (materialIt == g_scene.materials.end())
        continue;

      exporter::Material* bobaMaterial = *materialIt;
      prevMaterial = bobaMaterial->id;
      if (firstMaterial == ~0u)
        firstMaterial = bobaMaterial->id;
    }

    // Polygon Selection Tag
    if (btag->GetType() == Tpolygonselection && obj->GetType() == Opolygon)
    {
      // skip this selection tag if we can't find a previous material. i don't like relying things
      // just appearing in the correct order, but right now i don't know of a better way
      if (prevMaterial == ~0u)
      {
        continue;
      }

      if (melange::BaseSelect* bs = ((melange::SelectionTag*)btag)->GetBaseSelect())
      {
        auto& polysByMaterial = mesh->polysByMaterial[prevMaterial];
        for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
        {
          if (bs->IsSelected(i))
          {
            polysByMaterial.push_back(i);
            selectedPolys.insert(i);
          }
        }
      }

      // reset the previous material flag to avoid double selection tags causing trouble
      prevMaterial = ~0u;
    }
  }

  // if no materials are found, just add them to a dummy material
  if (firstMaterial == ~0u)
  {
    u32 dummyMaterial = DEFAULT_MATERIAL;
    auto& polysByMaterial = mesh->polysByMaterial[dummyMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      polysByMaterial.push_back(i);
    }
  }
  else
  {
    // add all the polygons that aren't selected to the first material
    auto& polysByMaterial = mesh->polysByMaterial[firstMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      if (selectedPolys.count(i) == 0)
        polysByMaterial.push_back(i);
    }
  }

  for (auto g : mesh->polysByMaterial)
  {
    const char* materialName =
      g.first == DEFAULT_MATERIAL ? "<default>" : g_scene.materials[g.first]->name.c_str();
    LOG(2, "material: %s, %d polys\n", materialName, (int)g.second.size());
  }
}

//-----------------------------------------------------------------------------
bool melange::AlienPolygonObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  PolygonObject* polyObj = (PolygonObject*)baseObj;

  exporter::Mesh* mesh = new exporter::Mesh(baseObj);
  CollectionAnimationTracks(baseObj, &mesh->animTracks);

  CollectMeshMaterials(polyObj, mesh);
  CollectVertices(polyObj, mesh);

#if WITH_XFORM_MTX
  CopyMatrix(polyObj->GetMl(), mesh->mtxLocal);
  CopyMatrix(polyObj->GetMg(), mesh->mtxGlobal);
#endif

  CopyTransform(polyObj->GetMl(), &mesh->xformLocal);
  CopyTransform(polyObj->GetMg(), &mesh->xformGlobal);

  if (mesh->valid)
  {
    g_scene.meshes.push_back(mesh);
  }
  else
  {
    delete mesh;
  }

  return true;
}
