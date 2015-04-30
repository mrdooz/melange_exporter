#include "boba_io.hpp"
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

    int GetFilePos()
    {
      return ftell(_f);
    }

    void SetFilePos(int p)
    {
      fseek(_f, p, SEEK_SET);
    }

    vector<DeferredData> _deferredData;
    deque<int> _filePosStack;
    u32 _deferredStartPos;
    FILE *_f;
  };

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

    writer.WriteRaw(&header, 4);
    writer.WriteDeferredStart();

    // write mesh start
    writer.Write(meshes.empty() ? (u32)0 : (u32)offsetof(boba::BobaScene, boba::BobaScene::data));

    // write camera start
    writer.Write((u32)0);

    // write light start
    writer.Write((u32)0);

    if (!meshes.empty())
    {
      // add # meshes
      writer.Write((u32)meshes.size());

      u32 numMeshes = (u32)meshes.size();

      // dummy write the meshes, to make sure the elements are in a contiguous block
      for (const Mesh* mesh : meshes)
      {
        writer.AddDeferredString(mesh->name);
        // Note, divide by 3 here to write # verts, and not # floats
        writer.Write((u32)mesh->verts.size() / 3);
        writer.Write((u32)mesh->indices.size());
        writer.AddDeferredVector(mesh->verts);
        if (mesh->normals.empty())
          writer.WritePtr(0);
        else
          writer.AddDeferredVector(mesh->normals);

        if (mesh->uv.empty())
          writer.WritePtr(0);
        else
          writer.AddDeferredVector(mesh->uv);
        writer.AddDeferredVector(mesh->indices);

        // save bounding box
        writer.Write(mesh->boundingSphere);
      }
    }

    writer.WriteDeferredData();

    return true;
  }

}

