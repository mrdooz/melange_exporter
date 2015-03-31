#include "exporter_helpers.hpp"

using namespace melange;

// overload this function and fill in your own unique data
void GetWriterInfo(Int32 &id, String &appname)
{
  // register your own pluginid once for your exporter and enter it here under id
  // this id must be used for your own unique ids
  // 	Bool AddUniqueID(LONG appid, const CHAR *const mem, LONG bytes);
  // 	Bool FindUniqueID(LONG appid, const CHAR *&mem, LONG &bytes) const;
  // 	Bool GetUniqueIDIndex(LONG idx, LONG &id, const CHAR *&mem, LONG &bytes) const;
  // 	LONG GetUniqueIDCount() const;
  id = 0;
  appname = "C4d -> boba converter";
}

namespace melange
{
  // memory allocation functions inside _melange_ namespace (if you have your own memory management you can overload these functions)
  // alloc memory no clear
  void *MemAllocNC(Int size)
  {
    void *mem = malloc(size);
    return mem;
  }

  // alloc memory set to 0
  void *MemAlloc(Int size)
  {
    void *mem = MemAllocNC(size);
    memset(mem, 0, size);
    return mem;
  }

  // realloc existing memory
  void *MemRealloc(void* orimem, Int size)
  {
    void *mem = realloc(orimem, size);
    return mem;
  }

  // free memory and set pointer to null
  void MemFree(void *&mem)
  {
    if (!mem)
      return;

    free(mem);
    mem = NULL;
  }
}
