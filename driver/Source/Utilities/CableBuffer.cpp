/*++

Module Name:

    CableBuffer.cpp

Abstract:

    GP Virtual Cable: ring buffer per cable (lihat CableBuffer.h).

--*/

#pragma warning (disable : 4127)

#include "definitions.h"
#include "endpoints.h"
#include "minipairs.h"
#include "gpring.h"
#include "CableBuffer.h"

// Konstanta di CableBuffer.h harus cocok dengan tabel hasil generator.
C_ASSERT(GP_MIX_CHANNELS == GP_CAPTURE_CHANNELS);
C_ASSERT(GP_CABLE_SAMPLE_RATE == CABLE_SAMPLE_RATE);
C_ASSERT(GP_CABLE_BYTES_PER_SAMPLE * 8 == CABLE_RENDER_BITS_PER_SAMPLE);
// Kapasitas dan batas latensi adalah jumlah frame utuh.
C_ASSERT(GP_CABLE_CAPACITY_FRAMES * 10 == GP_CABLE_SAMPLE_RATE);
C_ASSERT(GP_CABLE_LATENCY_LIMIT_FRAMES * 50 == GP_CABLE_SAMPLE_RATE);
C_ASSERT(GP_CABLE_LATENCY_LIMIT_FRAMES <= GP_CABLE_CAPACITY_FRAMES);

typedef struct _GP_CABLE
{
    KSPIN_LOCK  Lock;
    GpRing      Ring;           // Data == NULL / Capacity == 0 bila belum dialokasi
    ULONG       FrameBytes;     // 3 * channel
    ULONG       MaxFillBytes;   // batas latensi 20 ms, kelipatan FrameBytes
} GP_CABLE;

// Variabel global driver berada di memori NonPaged.
static GP_CABLE g_GpCables[GP_CABLE_COUNT];

//=============================================================================
#pragma code_seg()
ULONG GpCableCount()
{
    return GP_CABLE_COUNT;
}

#pragma code_seg()
ULONG GpCableChannels(ULONG cableIndex)
{
    if (cableIndex >= GP_CABLE_COUNT)
    {
        return 0;
    }
    return g_GpCableRoutes[cableIndex].Channels;
}

#pragma code_seg()
ULONG GpCableFirstChannel(ULONG cableIndex)
{
    if (cableIndex >= GP_CABLE_COUNT)
    {
        return 0;
    }
    return g_GpCableRoutes[cableIndex].FirstChannel;
}

//=============================================================================
#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS GpCablesInitialize()
{
    PAGED_CODE();

    for (ULONG i = 0; i < GP_CABLE_COUNT; i++)
    {
        GP_CABLE* c = &g_GpCables[i];
        KeInitializeSpinLock(&c->Lock);
        GpRingInit(&c->Ring, NULL, 0);
        c->FrameBytes = 0;
        c->MaxFillBytes = 0;
    }

    for (ULONG i = 0; i < GP_CABLE_COUNT; i++)
    {
        GP_CABLE* c = &g_GpCables[i];
        ULONG channels = g_GpCableRoutes[i].Channels;

        // Tabel dari generator hanya berisi cable mono/stereo yang muat di endpoint Recording.
        if (channels == 0 || channels > GP_CABLE_MAX_CHANNELS ||
            g_GpCableRoutes[i].FirstChannel + channels > GP_MIX_CHANNELS)
        {
            GpCablesCleanup();
            return STATUS_INVALID_DEVICE_STATE;
        }

        ULONG frameBytes = GP_CABLE_BYTES_PER_SAMPLE * channels;
        ULONG capacity = GP_CABLE_CAPACITY_FRAMES * frameBytes;
        BYTE* storage = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, capacity, GP_CABLE_POOLTAG);
        if (storage == NULL)
        {
            GpCablesCleanup();
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        c->FrameBytes = frameBytes;
        c->MaxFillBytes = GP_CABLE_LATENCY_LIMIT_FRAMES * frameBytes;
        GpRingInit(&c->Ring, storage, capacity);
    }

    return STATUS_SUCCESS;
}

//=============================================================================
// Non-paged karena memegang spin lock (DISPATCH_LEVEL). Dipanggil di PASSIVE_LEVEL.
#pragma code_seg()
VOID GpCablesCleanup()
{
    for (ULONG i = 0; i < GP_CABLE_COUNT; i++)
    {
        GP_CABLE* c = &g_GpCables[i];
        KIRQL oldIrql;
        BYTE* storage;

        KeAcquireSpinLock(&c->Lock, &oldIrql);
        storage = c->Ring.Data;
        GpRingInit(&c->Ring, NULL, 0);
        c->FrameBytes = 0;
        c->MaxFillBytes = 0;
        KeReleaseSpinLock(&c->Lock, oldIrql);

        if (storage != NULL)
        {
            ExFreePoolWithTag(storage, GP_CABLE_POOLTAG);
        }
    }
}

//=============================================================================
#pragma code_seg()
_Use_decl_annotations_
VOID GpCableWrite(ULONG cableIndex, const BYTE* src, ULONG n)
{
    if (cableIndex >= GP_CABLE_COUNT || src == NULL || n == 0)
    {
        return;
    }

    GP_CABLE* c = &g_GpCables[cableIndex];
    KIRQL oldIrql;

    KeAcquireSpinLock(&c->Lock, &oldIrql);
    if (c->FrameBytes != 0)
    {
        // Pemanggil selalu menulis frame utuh. Kapasitas ring kelipatan frame, jadi GpRingWrite
        // yang membuang byte tertua saat penuh juga hanya membuang frame utuh.
        NT_ASSERT(n % c->FrameBytes == 0);
        n -= n % c->FrameBytes;
        GpRingWrite(&c->Ring, src, n);
    }
    KeReleaseSpinLock(&c->Lock, oldIrql);
}

//=============================================================================
#pragma code_seg()
_Use_decl_annotations_
ULONG GpCableReadFrames(ULONG cableIndex, BYTE* dst, ULONG frames)
{
    if (cableIndex >= GP_CABLE_COUNT || dst == NULL || frames == 0)
    {
        return 0;
    }

    // Ukuran frame dari tabel (konstan), supaya dst tetap bisa dinolkan bila ring belum ada.
    ULONG frameBytes = GP_CABLE_BYTES_PER_SAMPLE * g_GpCableRoutes[cableIndex].Channels;
    if (frameBytes == 0 || frames > MAXULONG / frameBytes)
    {
        return 0;
    }
    ULONG bytes = frames * frameBytes;

    GP_CABLE* c = &g_GpCables[cableIndex];
    ULONG copied = 0;
    KIRQL oldIrql;

    KeAcquireSpinLock(&c->Lock, &oldIrql);
    if (c->FrameBytes == frameBytes)
    {
        ULONG fill = GpRingFill(&c->Ring);
        NT_ASSERT(fill % frameBytes == 0);

        // Kontrol latensi: buang bagian tertua; kelebihan dibulatkan KE ATAS ke frame utuh.
        if (fill > c->MaxFillBytes)
        {
            ULONG excessFrames = (fill - c->MaxFillBytes + frameBytes - 1) / frameBytes;
            GpRingDiscardTo(&c->Ring, fill - excessFrames * frameBytes);
        }

        copied = GpRingRead(&c->Ring, dst, bytes);   // sisa dst diisi nol
        NT_ASSERT(copied % frameBytes == 0);
    }
    else
    {
        RtlZeroMemory(dst, bytes);
    }
    KeReleaseSpinLock(&c->Lock, oldIrql);

    return copied / frameBytes;
}

//=============================================================================
#pragma code_seg()
_Use_decl_annotations_
VOID GpCableReset(ULONG cableIndex)
{
    if (cableIndex >= GP_CABLE_COUNT)
    {
        return;
    }

    GP_CABLE* c = &g_GpCables[cableIndex];
    KIRQL oldIrql;

    KeAcquireSpinLock(&c->Lock, &oldIrql);
    GpRingReset(&c->Ring);
    KeReleaseSpinLock(&c->Lock, oldIrql);
}
