// gpring.h - ring buffer byte murni (tanpa alokasi, lock, atau I/O). Pemanggil menyerahkan storage dan
// menyerialkan akses (spin lock di driver). Dapat dikompilasi di kernel mode dan user mode.
#ifndef GPRING_H
#define GPRING_H

#ifndef GP_MEM_MACROS
#define GP_MEM_MACROS
#if defined(_KERNEL_MODE)
#define GP_COPY(d, s, n) RtlCopyMemory((d), (s), (n))
#define GP_ZERO(d, n)    RtlZeroMemory((d), (n))
#else
#include <string.h>
#define GP_COPY(d, s, n) memcpy((d), (s), (n))
#define GP_ZERO(d, n)    memset((d), 0, (n))
#endif
#endif

#ifndef GP_CORE_TYPES
#define GP_CORE_TYPES
#if !defined(_WINDEF_) && !defined(_WINDOWS_)
typedef unsigned char BYTE;
#endif
#if !defined(_NTDEF_) && !defined(_WINNT_) && !defined(_WINDOWS_)
typedef unsigned char BOOLEAN;
typedef long LONG;
typedef unsigned long ULONG;
#endif
#endif

struct GpRing {
    BYTE* Data;
    ULONG Capacity;
    ULONG ReadPos;
    ULONG Fill;
};

__inline void GpRingReset(GpRing* r)
{
    r->ReadPos = 0;
    r->Fill = 0;
}

__inline void GpRingInit(GpRing* r, BYTE* storage, ULONG capacity)
{
    r->Data = storage;
    r->Capacity = capacity;
    GpRingReset(r);
}

__inline ULONG GpRingFill(const GpRing* r)
{
    return r->Fill;
}

// Penuh -> buang byte tertua. n > Capacity -> simpan Capacity byte terakhir.
__inline void GpRingWrite(GpRing* r, const BYTE* src, ULONG n)
{
    if (r->Capacity == 0 || n == 0) return;
    if (n >= r->Capacity) {
        src += n - r->Capacity;
        n = r->Capacity;
        r->ReadPos = 0;
        r->Fill = 0;
    } else if (r->Fill + n > r->Capacity) {
        ULONG drop = r->Fill + n - r->Capacity;
        r->ReadPos = (r->ReadPos + drop) % r->Capacity;
        r->Fill -= drop;
    }
    ULONG wpos = (r->ReadPos + r->Fill) % r->Capacity;
    ULONG first = r->Capacity - wpos;
    if (first > n) first = n;
    GP_COPY(r->Data + wpos, src, first);
    if (n > first) GP_COPY(r->Data, src + first, n - first);
    r->Fill += n;
}

// Salin min(n, Fill) byte, sisa dst diisi 0. Return jumlah byte yang tersalin.
__inline ULONG GpRingRead(GpRing* r, BYTE* dst, ULONG n)
{
    ULONG take = (n < r->Fill) ? n : r->Fill;
    if (take > 0) {
        ULONG first = r->Capacity - r->ReadPos;
        if (first > take) first = take;
        GP_COPY(dst, r->Data + r->ReadPos, first);
        if (take > first) GP_COPY(dst + first, r->Data, take - first);
        r->ReadPos = (r->ReadPos + take) % r->Capacity;
        r->Fill -= take;
    }
    if (n > take) GP_ZERO(dst + take, n - take);
    return take;
}

// Fill > maxFill -> buang byte tertua sampai Fill == maxFill.
__inline void GpRingDiscardTo(GpRing* r, ULONG maxFill)
{
    if (r->Fill > maxFill) {
        ULONG drop = r->Fill - maxFill;
        r->ReadPos = (r->ReadPos + drop) % r->Capacity;
        r->Fill = maxFill;
    }
}

#endif // GPRING_H
