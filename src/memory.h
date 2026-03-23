#ifndef MEMORY_H
#define MEMORY_H

#include "defs.h"
#include <windows.h> // For implementation, usually we'd separate but keeping single file for simplicity now

typedef struct memory_arena
{
    u8 *Base;
    u64 Size;
    u64 Used;
} memory_arena;

internal void
InitializeArena(memory_arena *Arena, u64 Size, void *Base)
{
    Arena->Base = (u8 *)Base;
    Arena->Size = Size;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))

internal void *
PushSize_(memory_arena *Arena, u64 Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#endif // MEMORY_H
