// gpmix.h - mixer penjumlah murni; sampel 24-bit packed little-endian bertanda. Tanpa alokasi/lock/I/O.
// Dapat dikompilasi di kernel mode dan user mode.
#ifndef GPMIX_H
#define GPMIX_H

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

#define GP_S24_MAX  8388607L
#define GP_S24_MIN  (-8388608L)

__inline LONG GpRead24(const BYTE* p)
{
    LONG v = (LONG)p[0] | ((LONG)p[1] << 8) | ((LONG)p[2] << 16);
    if (v & 0x800000L) v -= 0x1000000L;
    return v;
}

// acc[f*accChannels + firstChannel + c] += sampel. FALSE (tanpa menulis) bila di luar jangkauan.
__inline BOOLEAN GpMixAccumulate(LONG* acc, ULONG frames, ULONG accChannels,
                                 const BYTE* src24, ULONG srcChannels, ULONG firstChannel)
{
    if (srcChannels == 0 || firstChannel > accChannels || srcChannels > accChannels - firstChannel) return 0;
    for (ULONG f = 0; f < frames; f++) {
        for (ULONG c = 0; c < srcChannels; c++) {
            acc[f * accChannels + firstChannel + c] += GpRead24(src24 + (f * srcChannels + c) * 3);
        }
    }
    return 1;
}

// Saturasi ke 24-bit lalu tulis: 16 -> v>>8; 24 -> packed; 32 -> v<<8; bits lain -> nol.
__inline void GpMixWrite(const LONG* acc, ULONG frames, ULONG channels, BYTE* dst, ULONG bitsPerSample)
{
    ULONG count = frames * channels;
    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        GP_ZERO(dst, (size_t)count * ((bitsPerSample + 7) / 8));
        return;
    }
    for (ULONG i = 0; i < count; i++) {
        LONG v = acc[i];
        if (v > GP_S24_MAX) v = GP_S24_MAX;
        else if (v < GP_S24_MIN) v = GP_S24_MIN;
        if (bitsPerSample == 16) {
            LONG s = v >> 8;
            dst[0] = (BYTE)(s & 0xFF);
            dst[1] = (BYTE)((s >> 8) & 0xFF);
            dst += 2;
        } else if (bitsPerSample == 24) {
            dst[0] = (BYTE)(v & 0xFF);
            dst[1] = (BYTE)((v >> 8) & 0xFF);
            dst[2] = (BYTE)((v >> 16) & 0xFF);
            dst += 3;
        } else {
            ULONG s = (ULONG)v << 8;
            dst[0] = (BYTE)(s & 0xFF);
            dst[1] = (BYTE)((s >> 8) & 0xFF);
            dst[2] = (BYTE)((s >> 16) & 0xFF);
            dst[3] = (BYTE)((s >> 24) & 0xFF);
            dst += 4;
        }
    }
}

#endif // GPMIX_H
