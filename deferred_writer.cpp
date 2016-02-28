#include "deferred_writer.hpp"
#include <assert.h>
#include <string>

//------------------------------------------------------------------------------
DeferredWriter::DeferredWriter()
  : _f(nullptr)
  , _deferredStartPos(~0)
{
}

//------------------------------------------------------------------------------
DeferredWriter::~DeferredWriter()
{
  Close();
}

//------------------------------------------------------------------------------
bool DeferredWriter::Open(const char *filename)
{
#pragma warning(suppress: 4996)
  if (!(_f = fopen(filename, "wb")))
    return false;
  return true;
}

//------------------------------------------------------------------------------
void DeferredWriter::Close()
{
  if (_f)
    fclose(_f);
}

//------------------------------------------------------------------------------
void DeferredWriter::WritePtr(intptr_t ptr)
{
  Write(ptr);

  // to support both 32 and 64 bit reading, add a dummy 32 bits on 32-bit platforms
  if (sizeof(ptr) == 4)
    Write(0);
}

//------------------------------------------------------------------------------
void DeferredWriter::WriteDeferredStart()
{
  // write a placeholder for the deferred data starting pos
  _deferredStartPos = GetFilePos();
  Write((u32)0);
}

//------------------------------------------------------------------------------
u32 DeferredWriter::AddDeferredString(const std::string &str)
{
  return AddDeferredData(str.data(), (u32)str.size() + 1);
}

//------------------------------------------------------------------------------
u32 DeferredWriter::AddDeferredData(const void *data, u32 len)
{
  u32 tmp = DeferredDataSize();
  _deferredData.push_back(DeferredData(GetFilePos(), data, len));
  // dummy write, will be filled in later with the position of the actual deferred data
  WritePtr(0);
  return tmp;
}

//------------------------------------------------------------------------------
void DeferredWriter::WriteDeferredData()
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

  // save the references to the deferred data
  Write((int)(_deferredData.size() + _localFixups.size()));

  for (const DeferredData& d : _deferredData)
    Write(d.ref);

  for (const LocalFixup& lf : _localFixups)
    Write(lf.ref);

  // Save the deferred data
  for (const DeferredData& deferred : _deferredData)
  {
    int dataPos = GetFilePos();
    int len = (int)deferred.data.size();
    WriteRaw(&deferred.data[0], len);

    // Jump back to the position that references this blob, and update
    // its reference to point to the correct location
    PushFilePos();
    SetFilePos(deferred.ref);
    WritePtr(dataPos);
    PopFilePos();
  }

  for (const LocalFixup& lf : _localFixups)
  {
    PushFilePos();
    SetFilePos(lf.ref);
    WritePtr(lf.dst);
    PopFilePos();
  }
}

//------------------------------------------------------------------------------
void DeferredWriter::WriteRaw(const void *data, int len)
{
  fwrite(data, 1, len, _f);
}

//------------------------------------------------------------------------------
int DeferredWriter::GetFilePos() const
{
  return ftell(_f);
}

//------------------------------------------------------------------------------
void DeferredWriter::SetFilePos(int p)
{
  fseek(_f, p, SEEK_SET);
}

//------------------------------------------------------------------------------
void DeferredWriter::PushFilePos()
{
  _filePosStack.push_back(ftell(_f));
}

//------------------------------------------------------------------------------
void DeferredWriter::PopFilePos()
{
  assert(!_filePosStack.empty());
  int p = _filePosStack.back();
  _filePosStack.pop_back();
  fseek(_f, p, SEEK_SET);
}

//------------------------------------------------------------------------------
u32 DeferredWriter::DeferredDataSize() const
{
  u32 res = 0;
  for (const DeferredData& d : _deferredData)
    res += (u32)d.data.size();
  return res;
}

//------------------------------------------------------------------------------
u32 DeferredWriter::CreateFixup()
{
  // create a pending fixup at FilePos
  u32 f = _nextFixup++;
  _pendingFixups[f] = GetFilePos();
  WritePtr(0);
  return f;
}

//------------------------------------------------------------------------------
void DeferredWriter::InsertFixup(u32 id)
{
  auto it = _pendingFixups.find(id);
  assert(it != _pendingFixups.end());

  // marks the the data for id is going to be written at FilePos
  _localFixups.push_back({ it->second, (u32)GetFilePos() });
  _pendingFixups.erase(it);
}

//------------------------------------------------------------------------------
void DeferredWriter::StartBlockMarker()
{
  _blockStack.push_back(GetFilePos());
  // dummy write
  Write((u32)0);
}

//------------------------------------------------------------------------------
void DeferredWriter::EndBlockMarker()
{
  u32 start = _blockStack.back();
  _blockStack.pop_back();
  u32 blockSize = GetFilePos() - start;
  PushFilePos();
  SetFilePos(start);
  Write(blockSize);
  PopFilePos();
}
