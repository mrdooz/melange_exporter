#pragma once

namespace exporter
{
  struct Transform;
}

void CopyTransform(const melange::Matrix& mtx, exporter::Transform* xform);
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
void GetChildren(melange::BaseObject* obj, vector<melange::BaseObject*>* children);
