#pragma once
#include <c4d_file.h>

namespace melange
{
  // memory allocation functions inside _melange_ namespace (if you have your own memory management you can overload these functions)
  // alloc memory no clear
  void *MemAllocNC(Int size);

  // alloc memory set to 0
  void *MemAlloc(Int size);

  // realloc existing memory
  void *MemRealloc(void* orimem, Int size);

  // free memory and set pointer to null
  void MemFree(void *&mem);
}

// overload this function and fill in your own unique data
void GetWriterInfo(melange::Int32 &id, melange::String &appname);
