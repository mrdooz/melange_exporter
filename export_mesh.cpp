#include "export_mesh.hpp"
#include "exporter.hpp"
#include "export_misc.hpp"
#include "exporter_utils.hpp"

static melange::AlienMaterial* DEFAULT_MATERIAL_PTR = nullptr;
//-----------------------------------------------------------------------------
template <typename Dst, typename Src>
Dst Vector3Coerce(const Src& src)
{
  return Dst(src.x, src.y, src.z);
}

//-----------------------------------------------------------------------------
template <typename Ret, typename Src>
Ret AlphabetIndex(const Src& src, int idx)
{
  switch (idx)
  {
  case 0: return src.a;
  case 1: return src.b;
  case 2: return src.c;
  case 3: return src.d;
  }

  return Ret();
}

//-----------------------------------------------------------------------------
static int VertexIdFromPoly(const melange::CPolygon& poly, int idx)
{
  switch (idx)
  {
    case 0: return poly.a;
    case 1: return poly.b;
    case 2: return poly.c;
    case 3: return poly.d;
  }
  return -1;
}

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
void CopyOutVector2(float* dst, const T& src)
{
  dst[0] = src.x;
  dst[1] = src.y;
}

//-----------------------------------------------------------------------------
template <typename T>
void CopyOutVector3(float* dst, const T& src)
{
  dst[0] = src.x;
  dst[1] = src.y;
  dst[2] = src.z;
}

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
static void CalcBoundingSphere(
    const melange::Vector* verts, int vertexCount, exporter::Vec3f* outCenter, float* outRadius)
{
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

  *outCenter = center;
  *outRadius = sqrtf(radius);
}

static u32 FnvHash(const char* str, u32 d = 0x01000193)
{
  while (true)
  {
    char c = *str++;
    if (!c)
      return d;
    d = ((d * 0x01000193) ^ c) & 0xffffffff;
  }
}

//-----------------------------------------------------------------------------
struct FatVertex
{
  FatVertex()
  {
    memset(this, 0, sizeof(FatVertex));
  }

  melange::Vector32 pos = melange::Vector32(0,0,0);
  melange::Vector32 normal = melange::Vector32(0,0,0);
  melange::Vector32 uv = melange::Vector32(0,0,0);

  u32 GetHash() const
  {
    if (meta.hash == 0)
      meta.hash = FnvHash((const char*)this, sizeof(FatVertex) - sizeof(meta));
    return meta.hash;
  }

  friend bool operator==(const FatVertex& lhs, const FatVertex& rhs)
  {
    return lhs.pos == rhs.pos && lhs.normal == rhs.normal && lhs.uv == rhs.uv;
  }

  struct Hash
  {
    size_t operator()(const FatVertex& v) const
    {
      return v.GetHash();
    }
  };

  struct
  {
    int id = 0;
    mutable u32 hash = 0;
  } meta;
};

//-----------------------------------------------------------------------------
static void GroupPolysByMaterial(melange::PolygonObject* obj,
    unordered_map<melange::AlienMaterial*, vector<int>>* polysByMaterial)
{
  // Keep track of which polys we've seen, so we can lump all the unseen ones with the first
  // material
  unordered_set<u32> seenPolys;

  melange::AlienMaterial* prevMaterial = nullptr;
  melange::AlienMaterial* firstMaterial = nullptr;

  // For each material found, check if the following tag is a selection tag, in which
  // case record which polys belong to it
  for (melange::BaseTag* btag = obj->GetFirstTag(); btag; btag = btag->GetNext())
  {
    // texture tag
    if (btag->GetType() == Ttexture)
    {
      melange::GeData data;
      prevMaterial = btag->GetParameter(melange::TEXTURETAG_MATERIAL, data)
        ? (melange::AlienMaterial*)data.GetLink()
        : NULL;
      if (!prevMaterial)
        continue;

      // Mark the first material we find, so we can stick all unselected polys in it
      if (!firstMaterial)
        firstMaterial = prevMaterial;
    }

    // Polygon Selection Tag
    if (btag->GetType() == Tpolygonselection && obj->GetType() == Opolygon)
    {
      // skip this selection tag if we don't have a previous material. i don't like relying things
      // just appearing in the correct order, but right now i don't know of a better way
      if (!prevMaterial)
        continue;

      if (melange::BaseSelect* bs = ((melange::SelectionTag*)btag)->GetBaseSelect())
      {
        vector<int>& polysForMaterial = (*polysByMaterial)[prevMaterial];
        for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
        {
          if (bs->IsSelected(i))
          {
            polysForMaterial.push_back(i);
            seenPolys.insert(i);
          }
        }
      }

      // reset the previous material flag to avoid double selection tags causing trouble
      prevMaterial = nullptr;
    }
  }

  // if no materials are found, just add them to a dummy material
  if (!firstMaterial)
  {
    vector<int>& polysForMaterial = (*polysByMaterial)[DEFAULT_MATERIAL_PTR];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      polysForMaterial.push_back(i);
    }
  }
  else
  {
    // add all the polygons that aren't selected to the first material
    vector<int>& polysForMaterial = (*polysByMaterial)[firstMaterial];
    for (int i = 0, e = obj->GetPolygonCount(); i < e; ++i)
    {
      if (seenPolys.count(i) == 0)
        polysForMaterial.push_back(i);
    }
  }

  // print polys per material stats.
  for (auto g : (*polysByMaterial))
  {
    melange::AlienMaterial* mat = g.first;
    const char* materialName =
      mat == DEFAULT_MATERIAL_PTR ? "<default>" : CopyString(mat->GetName()).c_str();
    LOG(2, "material: %s, %d polys\n", materialName, (int)g.second.size());
  }
}

//-----------------------------------------------------------------------------
struct FatVertexSupplier
{
  FatVertexSupplier(melange::PolygonObject* polyObj)
  {
    hasNormalsTag = !!polyObj->GetTag(Tnormal);
    hasPhongTag = !!polyObj->GetTag(Tphong);
    hasNormals = hasNormalsTag || hasPhongTag;

    phongNormals = hasPhongTag ? polyObj->CreatePhongNormals() : nullptr;
    normals = hasNormalsTag ? (melange::NormalTag*)polyObj->GetTag(Tnormal) : nullptr;
    uvs = (melange::UVWTag*)polyObj->GetTag(Tuvw);
    uvHandle = uvs ? uvs->GetDataAddressR() : nullptr;
    normalHandle = normals ? normals->GetDataAddressR() : nullptr;

    verts = polyObj->GetPointR();
    polys = polyObj->GetPolygonR();
  }

  ~FatVertexSupplier()
  {
    if (phongNormals)
      melange::_MemFree((void**)&phongNormals);
  }

  int AddVertex(int polyIdx, int vertIdx)
  {
    const melange::CPolygon& poly = polys[polyIdx];

    FatVertex vtx;
    vtx.pos = Vector3Coerce<melange::Vector32>(verts[AlphabetIndex<int>(poly, vertIdx)]);

    if (hasNormals)
    {
      if (hasNormalsTag)
      {
        melange::NormalStruct normal;
        normals->Get(normalHandle, polyIdx, normal);
        vtx.normal = Vector3Coerce<melange::Vector32>(AlphabetIndex<melange::Vector>(normal, vertIdx));
      }
      else if (hasPhongTag)
      {
        vtx.normal = phongNormals[polyIdx*4+vertIdx];
      }
    }
    else
    {
      // no normals, so generate polygon normals and use them for all verts
      int idx0 = AlphabetIndex<int>(poly, 0);
      int idx1 = AlphabetIndex<int>(poly, 1);
      int idx2 = AlphabetIndex<int>(poly, 2);
      vtx.normal = Vector3Coerce<melange::Vector32>(CalcNormal(verts[idx0], verts[idx1], verts[idx2]));
    }

    if (uvHandle)
    {
      melange::UVWStruct s;
      melange::UVWTag::Get(uvHandle, polyIdx, s);
      vtx.uv = Vector3Coerce<melange::Vector32>(AlphabetIndex<melange::Vector>(s, vertIdx));
    }

    // Check if the fat vertex already exists
    auto it = fatVertSet.find(vtx);
    if (it != fatVertSet.end())
      return it->meta.id;

    vtx.meta.id = (int)fatVerts.size();
    fatVertSet.insert(vtx);
    fatVerts.push_back(vtx);
    return vtx.meta.id;
  }

  const melange::Vector* verts;
  const melange::CPolygon* polys;

  bool hasNormalsTag;
  bool hasPhongTag;
  bool hasNormals;

  melange::Vector32* phongNormals;
  melange::NormalTag* normals;
  melange::UVWTag* uvs;
  melange::ConstUVWHandle uvHandle;
  melange::ConstNormalHandle normalHandle;

  unordered_set<FatVertex, FatVertex::Hash> fatVertSet;
  vector<FatVertex> fatVerts;
};

//-----------------------------------------------------------------------------
struct DataStreamHelper
{
  DataStreamHelper()
    : used(0)
    , size(16*1024)
  {
    data.resize(size);
  }

  template <typename T>
  void Add(const T& t)
  {
    int required = used + sizeof(T);
    if (required > size)
    {
      size = max(required, (int)size) * 1.5;
      data.resize(size);
    }

    memcpy(&data[used], &t, sizeof(T));
    used += sizeof(T);
  }

  void CopyOut(const string& name, exporter::Mesh* mesh)
  {
    mesh->dataStreams.push_back(exporter::Mesh::DataStream());
    exporter::Mesh::DataStream& s = mesh->dataStreams.back();
    s.name = name;
    s.flags = 0;
    s.data.resize(used);
    memcpy(s.data.data(), data.data(), used);
  }

  vector<char> data;
  int used = 0;
  int size = 0;
};

//-----------------------------------------------------------------------------
static void CollectVertices(melange::PolygonObject* polyObj,
    const unordered_map<melange::AlienMaterial*, vector<int>>& polysByMaterial,
    exporter::Mesh* mesh)
{
  int vertexCount = polyObj->GetPointCount();
  if (!vertexCount)
  {
    // TODO: log
    return;
  }

  const melange::Vector* verts = polyObj->GetPointR();
  const melange::CPolygon* polys = polyObj->GetPolygonR();

  CalcBoundingSphere(
    verts, vertexCount, &mesh->boundingSphere.center, &mesh->boundingSphere.radius);

  FatVertexSupplier fatVtx(polyObj);
  int startIdx = 0;

  DataStreamHelper indexStream;

  // Create the material groups, where each group contains polygons that share the same material
  for (const pair<melange::AlienMaterial*, vector<int>>& kv : polysByMaterial)
  {
    exporter::Mesh::MaterialGroup mg;
    mg.mat = kv.first;
    mg.startIndex = startIdx;

    // iterate over all the polygons in the material group, and collect the vertices
    for (int polyIdx : kv.second)
    {
      int idx0 = fatVtx.AddVertex(polyIdx, 0);
      int idx1 = fatVtx.AddVertex(polyIdx, 1);
      int idx2 = fatVtx.AddVertex(polyIdx, 2);

      indexStream.Add(idx0);
      indexStream.Add(idx1);
      indexStream.Add(idx2);
      startIdx += 3;

      if (IsQuad(polys[polyIdx]))
      {
        int idx3 = fatVtx.AddVertex(polyIdx, 3);
        indexStream.Add(idx0);
        indexStream.Add(idx2);
        indexStream.Add(idx3);
        startIdx += 3;
      }
    }
    mg.numIndices = startIdx - mg.startIndex;
    mesh->materialGroups.push_back(mg);
  }

  // copy the data over from the fat vertices
  int numFatVerts = (int)fatVtx.fatVerts.size();

  DataStreamHelper posStream;
  DataStreamHelper normalStream;
  DataStreamHelper uvStream;

  for (int i = 0; i < numFatVerts; ++i)
  {
    posStream.Add(fatVtx.fatVerts[i].pos);
    normalStream.Add(fatVtx.fatVerts[i].normal);
    if (fatVtx.uvHandle)
    {
      uvStream.Add(fatVtx.fatVerts[i].uv.x);
      uvStream.Add(fatVtx.fatVerts[i].uv.y);
    }
  }

  indexStream.CopyOut("index32", mesh);
  posStream.CopyOut("pos", mesh);
  normalStream.CopyOut("normal", mesh);
  if (uvStream.size)
  {
    uvStream.CopyOut("uv", mesh);
  }
}

//-----------------------------------------------------------------------------
bool melange::AlienPolygonObjectData::Execute()
{
  BaseObject* baseObj = (BaseObject*)GetNode();
  PolygonObject* polyObj = (PolygonObject*)baseObj;

  exporter::Mesh* mesh = new exporter::Mesh(baseObj);

  unordered_map<melange::AlienMaterial*, vector<int>> polysByMaterial;
  GroupPolysByMaterial(polyObj, &polysByMaterial);
  CollectVertices(polyObj, polysByMaterial, mesh);

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
