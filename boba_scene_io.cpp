#include "boba_scene_io.hpp"
#include <deque>
#include <assert.h>

namespace boba
{
  u32 Material::nextId;

  class DeferredWriter
  {
  public:
    DeferredWriter()
      : _f(nullptr)
      , _deferredStartPos(~0)
    {
    }

    ~DeferredWriter()
    {
      Close();
    }

    struct DeferredData
    {
      DeferredData(u32 ref, const void *d, u32 len, bool saveBlobSize)
        : ref(ref)
        , saveBlobSize(saveBlobSize)
      {
        data.resize(len);
        memcpy(&data[0], d, len);
      }

      // The position in the file that references the deferred block
      u32 ref;
      bool saveBlobSize;
      vector<u8> data;
    };

    bool Open(const char *filename)
    {
#pragma warning(suppress: 4996)
      if (!(_f = fopen(filename, "wb")))
        return false;
      return true;
    }

    void Close()
    {
      if (_f)
        fclose(_f);
    }

    void WritePtr(intptr_t ptr)
    {
      Write(ptr);

      // to support both 32 and 64 bit reading, add a dummy 32 bits on 32-bit platforms
      if (sizeof(ptr) == 4)
        Write(0);
    }

    void WriteDeferredStart()
    {
      // write a placeholder for the deferred data starting pos
      _deferredStartPos = GetFilePos();
      Write((u32)0);
    }

    void AddDeferredString(const std::string &str)
    {
      _deferredData.push_back(DeferredData(GetFilePos(), str.data(), (u32)str.size() + 1, false));
      // dummy write, will be filled in later with the position of the actual deferred data
      WritePtr(0);
    }

    void AddDeferredData(const void *data, u32 len)
    {
      _deferredData.push_back(DeferredData(GetFilePos(), data, len, true));
      WritePtr(0);
    }

    template<typename T>
    void AddDeferredVector(const vector<T>& v)
    {
      // todo: writing the length should probably be exposed as part of the API
      _deferredData.push_back(DeferredData(GetFilePos(), v.data(), (u32)v.size() * sizeof(T), false));
      WritePtr(0);
    }

    void WriteDeferredData()
    {
      int deferredStart = GetFilePos();

      if (_deferredStartPos != ~0)
      {
        // Write back the deferred data starting pos
        PushFilePos();
        SetFilePos(_deferredStartPos);
        Write(deferredStart);
        PopFilePos();
      }

      Write((int)_deferredData.size());

      // save the references to the deferred data
      for (size_t i = 0; i < _deferredData.size(); ++i)
        Write(_deferredData[i].ref);

      for (const DeferredData& deferred : _deferredData)
      {
        int dataPos = GetFilePos();
        int len = (int)deferred.data.size();
        if (deferred.saveBlobSize)
          Write(len);
        WriteRaw(&deferred.data[0], len);

        // Jump back to the position that references this blob, and update
        // it's reference to point to the correct location
        PushFilePos();
        SetFilePos(deferred.ref);
        WritePtr(dataPos);
        PopFilePos();
      }
    }

    template <class T>
    void Write(const T& data) const
    {
      fwrite(&data, 1, sizeof(T), _f);
    }

    void WriteRaw(const void *data, int len)
    {
      fwrite(data, 1, len, _f);
    }

    int GetFilePos()
    {
      return ftell(_f);
    }

    void SetFilePos(int p)
    {
      fseek(_f, p, SEEK_SET);
    }

  private:
    void PushFilePos()
    {
      _filePosStack.push_back(ftell(_f));
    }

    void PopFilePos()
    {
      assert(!_filePosStack.empty());
      int p = _filePosStack.back();
      _filePosStack.pop_back();
      fseek(_f, p, SEEK_SET);
    }

    vector<DeferredData> _deferredData;
    deque<int> _filePosStack;
    // Where in the file should we write the location of the deferred data (this is not the location itself)
    u32 _deferredStartPos;
    FILE *_f;
  };

  //------------------------------------------------------------------------------
#ifndef _WIN32
  template<typename T, typename U> constexpr size_t offsetOf(U T::*member)
  {
    return (char*)&((T*)nullptr->*member) - (char*)nullptr;
  }
#endif
  //------------------------------------------------------------------------------
  bool Scene::Save(const char* filename)
  {
    DeferredWriter writer;
    if (!writer.Open(filename))
      return false;

    boba::BobaScene header;
    header.id[0] = 'b';
    header.id[1] = 'o';
    header.id[2] = 'b';
    header.id[3] = 'a';

    // dummy write the header
    writer.Write(header);

    #ifdef _WIN32
    header.meshDataStart = meshes.empty() ? (u32)0 : (u32)offsetof(boba::BobaScene, boba::BobaScene::data);
    #else
    header.meshDataStart = meshes.empty() ? (u32)0 : offsetOf(&boba::BobaScene::data);
    #endif
    header.numMeshes = (u32)meshes.size();

    // write camera start
    header.cameraDataStart = 0;
    header.numCameras = 0;

    // write light start
    header.lightDataStart = 0;
    header.numLights = 0;

    if (!meshes.empty())
    {
      // add # meshes
      for (Mesh* mesh : meshes)
      {
        mesh->Save(writer);
      }
    }

    header.fixupOffset = (u32)writer.GetFilePos();
    writer.WriteDeferredData();

    // write back the correct header
    writer.SetFilePos(0);
    writer.Write(header);

    return true;
  }

  //------------------------------------------------------------------------------
  void Mesh::Save(DeferredWriter& writer)
  {
    writer.AddDeferredString(name);
    // Note, divide by 3 here to write # verts, and not # floats
    writer.Write((u32)verts.size() / 3);
    writer.Write((u32)indices.size());
    writer.AddDeferredVector(verts);
    if (normals.empty())
      writer.WritePtr(0);
    else
      writer.AddDeferredVector(normals);

    if (uv.empty())
      writer.WritePtr(0);
    else
      writer.AddDeferredVector(uv);
    writer.AddDeferredVector(indices);

    // save bounding box
    writer.Write(boundingSphere);

  }

}
