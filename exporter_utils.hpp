#pragma once

namespace exporter
{
  struct Transform;
}

namespace scene
{
  struct Transform;
}

//-----------------------------------------------------------------------------
void CopyTransform(const melange::Matrix& mtx, exporter::Transform* xform);
void CopyTransform(const melange::Matrix& mtx, scene::Transform* xform);
void CopyMatrix(const melange::Matrix& mtx, float* out);

string CopyString(const melange::String& str);

#define LOG(lvl, fmt, ...)                                                                         \
  if (options.loglevel >= lvl)                                                                     \
    printf(fmt, __VA_ARGS__);                                                                      \
  if (options.logfile)                                                                             \
    fprintf(options.logfile, fmt, __VA_ARGS__);

//-----------------------------------------------------------------------------
template <typename R, typename T>
R GetVectorParam(T* obj, int paramId)
{
  melange::GeData data;
  obj->GetParameter(paramId, data);
  melange::Vector v = data.GetVector();
  return R(v.x, v.y, v.z);
}

//-----------------------------------------------------------------------------
template <typename R, typename T>
R GetColorParam(T* obj, int paramId)
{
  melange::GeData data;
  obj->GetParameter(paramId, data);
  melange::Vector v = data.GetVector();
  return R{v.x, v.y, v.z, 1};
}

//-----------------------------------------------------------------------------
template <typename T>
float GetFloatParam(T* obj, int paramId)
{
  melange::GeData data;
  obj->GetParameter(paramId, data);
  return (float)data.GetFloat();
}

//-----------------------------------------------------------------------------
template <typename T>
int GetInt32Param(T* obj, int paramId)
{
  melange::GeData data;
  obj->GetParameter(paramId, data);
  return (float)data.GetInt32();
}

//-----------------------------------------------------------------------------
template<typename T, typename U>
T* SharedPtrCast(shared_ptr<U>& ptr)
{
  return static_cast<T*>(ptr.get());
}

//-----------------------------------------------------------------------------
void GetChildren(melange::BaseObject* obj, vector<melange::BaseObject*>* children);

struct IdGenerator
{
  IdGenerator(int initialId = 0) : _nextId(initialId) {}
  int NextId() { return _nextId++; }
  int _nextId;
};

extern IdGenerator g_ObjectId;
extern IdGenerator g_MaterialId;
extern unordered_map<int, melange::BaseMaterial*> g_MaterialIdToObj;
