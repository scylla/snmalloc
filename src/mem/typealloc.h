#pragma once

#include "../ds/flaglock.h"
#include "../ds/mpmcstack.h"
#include "typeallocated.h"

namespace snmalloc
{
  template<class T, class MemoryProvider = GlobalVirtual>
  class TypeAlloc
  {
  private:
    friend TypeAllocated<T>;

    std::atomic_flag lock = ATOMIC_FLAG_INIT;
    MPMCStack<T, PreZeroed> stack;
    T* list = nullptr;

    TypeAlloc(MemoryProvider& m) : memory_provider(m) {}

  public:
    MemoryProvider& memory_provider;

    static TypeAlloc* make(MemoryProvider& memory_provider) noexcept
    {
      auto r = memory_provider.alloc_chunk(sizeof(TypeAlloc));
      return new (r) TypeAlloc(memory_provider);
    }

    static TypeAlloc* make() noexcept
    {
      return make(default_memory_provider);
    }

    template<typename... Args>
    T* alloc(Args&&... args)
    {
      T* p = stack.pop();

      if (p != nullptr)
        return p;

      p = (T*)memory_provider.alloc_chunk(sizeof(T));

      new (p) T(std::forward<Args...>(args)...);

      FlagLock f(lock);
      p->list_next = list;
      list = p;

      return p;
    }

    void dealloc(T* p)
    {
      // The object's destructor is not run. If the object is "reallocated", it
      // is returned without the constructor being run, so the object is reused
      // without re-initialisation.
      stack.push(p);
    }

    T* extract(T* p = nullptr)
    {
      // Returns a linked list of all objects in the stack, emptying the stack.
      if (p == nullptr)
        return stack.pop_all();
      else
        return p->next;
    }

    void restore(T* first, T* last)
    {
      // Pushes a linked list of objects onto the stack. Use to put a linked
      // list returned by extract back onto the stack.
      stack.push(first, last);
    }

    T* iterate(T* p = nullptr)
    {
      if (p == nullptr)
        return list;
      else
        return p->list_next;
    }
  };
}
