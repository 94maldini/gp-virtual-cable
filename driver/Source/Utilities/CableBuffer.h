/*++

Module Name:

    CableBuffer.h

Abstract:

    GP Virtual Cable: satu ring buffer NonPaged per cable (Playback). Sisi render menulis byte DMA ke
    ring cable-nya; sisi capture membaca jumlah frame yang sama dari tiap ring lalu menjumlahkannya.

    Aturan:
    - Isi ring selalu kelipatan utuh frame cable itu (3 * channel byte): kapasitas, setiap tulis,
      baca dan buang adalah kelipatan frame, sehingga GpRingWrite yang membuang byte tertua saat
      penuh hanya pernah membuang frame utuh.
    - Satu KSPIN_LOCK per cable; fungsi tulis/baca/reset memegang paling banyak satu lock cable dan
      tidak mengalokasi memori (aman di DISPATCH_LEVEL / DPC).
    - Antarmuka ini tidak membocorkan tabel minipair hasil generator; nilai rute dibaca lewat
      GpCableCount/GpCableChannels/GpCableFirstChannel (dicocokkan dengan C_ASSERT di CableBuffer.cpp).
--*/

#ifndef _GP_CABLEBUFFER_H_
#define _GP_CABLEBUFFER_H_

#define GP_CABLE_POOLTAG                'cbPG'

#define GP_CABLE_SAMPLE_RATE            48000
#define GP_CABLE_BYTES_PER_SAMPLE       3       // 24-bit packed
#define GP_CABLE_CAPACITY_FRAMES        (GP_CABLE_SAMPLE_RATE / 10)     // 100 ms
#define GP_CABLE_LATENCY_LIMIT_FRAMES   (GP_CABLE_SAMPLE_RATE / 50)     // 20 ms

// Geometri pencampuran (dicek dengan C_ASSERT terhadap cables_generated.h di CableBuffer.cpp).
#define GP_CABLE_MAX_CHANNELS           2       // cable Playback: mono atau stereo
#define GP_MIX_CHANNELS                 8       // channel endpoint Recording
#define GP_MIX_CHUNK_FRAMES             480     // 10 ms @ 48 kHz; ukuran akumulator
#define GP_CABLE_MAX_FRAME_BYTES        (GP_CABLE_BYTES_PER_SAMPLE * GP_CABLE_MAX_CHANNELS)
#define GP_MIX_SCRATCH_BYTES_PER_CABLE  (GP_MIX_CHUNK_FRAMES * GP_CABLE_MAX_FRAME_BYTES)

// Jumlah cable Playback.
ULONG    GpCableCount();

// Jumlah channel cable (0 untuk indeks invalid).
ULONG    GpCableChannels(ULONG cableIndex);

// Channel pertama (0-based) cable ini di endpoint Recording (0 untuk indeks invalid).
ULONG    GpCableFirstChannel(ULONG cableIndex);

// Alokasi ring NonPaged (ExAllocatePool2, tag 'cbPG') per cable, kapasitas 100 ms. PASSIVE_LEVEL.
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS GpCablesInitialize();

// Bebaskan semua ring; tidak boleh ada stream yang masih berjalan. PASSIVE_LEVEL.
VOID     GpCablesCleanup();

// Tulis n byte (kelipatan frame cable) ke ring cable. Indeks invalid -> abaikan.
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID     GpCableWrite(ULONG cableIndex, _In_reads_bytes_(n) const BYTE* src, ULONG n);

// Buang bagian tertua sampai isi <= 20 ms (kelebihan dibulatkan KE ATAS ke frame utuh), lalu baca
// frames * 3 * channel byte ke dst; bagian yang tidak tersedia diisi nol, jadi dst selalu terisi
// penuh untuk indeks yang valid. Return jumlah frame yang benar-benar tersalin dari ring.
// Indeks invalid -> return 0 tanpa menyentuh dst.
_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG    GpCableReadFrames(ULONG cableIndex, BYTE* dst, ULONG frames);

// Kosongkan ring cable. Indeks invalid -> abaikan.
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID     GpCableReset(ULONG cableIndex);

#endif // _GP_CABLEBUFFER_H_
