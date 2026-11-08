/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minwavertstream.h

Abstract:

    Definition of wavert miniport class.
--*/

#ifndef _SIMPLEAUDIOSAMPLE_MINWAVERTSTREAM_H_
#define _SIMPLEAUDIOSAMPLE_MINWAVERTSTREAM_H_

#include "CableBuffer.h"

//
// Structure to store notifications events in a protected list
//
typedef struct _NotificationListEntry
{
    LIST_ENTRY  ListEntry;
    PKEVENT     NotificationEvent;
} NotificationListEntry;

EXT_CALLBACK   TimerNotifyRT;

//=============================================================================
// Referenced Forward
//=============================================================================
class CMiniportWaveRT;
typedef CMiniportWaveRT *PCMiniportWaveRT;

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRTStream 
// 
class CMiniportWaveRTStream : 
    public IDrmAudioStream,
    public IMiniportWaveRTStreamNotification,
    public IMiniportWaveRTInputStream,
    public IMiniportWaveRTOutputStream,
    public CUnknown
{
protected:
    PPORTWAVERTSTREAM           m_pPortStream;
    LIST_ENTRY                  m_NotificationList;
    PEX_TIMER                   m_pNotificationTimer;
    ULONG                       m_ulNotificationIntervalMs;
    ULONG                       m_ulCurrentWritePosition;
    LONG                        m_IsCurrentWritePositionUpdated;
    
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTStream);
    ~CMiniportWaveRTStream();

    IMP_IMiniportWaveRTStream;
    IMP_IMiniportWaveRTStreamNotification;
    IMP_IMiniportWaveRTInputStream;
    IMP_IMiniportWaveRTOutputStream;
    IMP_IMiniportWaveRT;
    IMP_IDrmAudioStream;

    NTSTATUS                    Init
    ( 
        _In_  PCMiniportWaveRT    Miniport,
        _In_  PPORTWAVERTSTREAM   Stream,
        _In_  ULONG               Channel,
        _In_  BOOLEAN             Capture,
        _In_  PKSDATAFORMAT       DataFormat,
        _In_  GUID                SignalProcessingMode
    );

    // Friends
    friend class                CMiniportWaveRT;
    friend EXT_CALLBACK         TimerNotifyRT;
protected:
    CMiniportWaveRT*            m_pMiniport;
    ULONG                       m_ulPin;
    BOOLEAN                     m_bCapture;
    BOOLEAN                     m_bUnregisterStream;
    ULONG                       m_ulDmaBufferSize;
    BYTE*                       m_pDmaBuffer;
    ULONG                       m_ulNotificationsPerBuffer;
    KSSTATE                     m_KsState;
    PKTIMER                     m_pTimer;
    PRKDPC                      m_pDpc;
    ULONGLONG                   m_ullPlayPosition;
    ULONGLONG                   m_ullWritePosition;
    ULONGLONG                   m_ullLinearPosition;
    ULONGLONG                   m_ullPresentationPosition;
    ULONG                       m_ulLastOsReadPacket;
    ULONG                       m_ulLastOsWritePacket;
    LONGLONG                    m_llPacketCounter;
    ULONGLONG                   m_ullDmaTimeStamp;
    LARGE_INTEGER               m_ullPerformanceCounterFrequency;
    ULONGLONG                   m_hnsElapsedTimeCarryForward;
    ULONGLONG                   m_ullLastDPCTimeStamp;
    ULONGLONG                   m_hnsDPCTimeCarryForward;
    ULONG                       m_byteDisplacementCarryForward;
    ULONG                       m_ulDmaMovementRate;
    BOOL                        m_bLfxEnabled;
    PBOOL                       m_pbMuted;
    PLONG                       m_plVolumeLevel;
    PLONG                       m_plPeakMeter;
    PWAVEFORMATEXTENSIBLE       m_pWfExt;
    ULONG                       m_ulContentId;
    GUID                        m_SignalProcessingMode;
    BOOLEAN                     m_bEoSReceived;
    BOOLEAN                     m_bLastBufferRendered;
    KSPIN_LOCK                  m_PositionSpinLock;

    // GP Virtual Cable. Diisi di Init (sebelum stream berjalan); selama berjalan hanya disentuh di
    // bawah m_PositionSpinLock (UpdatePosition, SetState STOP), kecuali m_bGpCopyProtect yang
    // ditulis SetContentId (satu BOOLEAN; DPC membacanya apa adanya).
    ULONG                       m_ulGpCableIndex = MAXULONG;    // render: indeks cable; capture: GP_CAPTURE_CABLE_INDEX
    ULONG                       m_ulGpFrameBytes = 0;   // nBlockAlign format stream
    ULONG                       m_ulGpBitsPerSample = 0;    // capture: 16, 24 atau 32 (container)
    BOOLEAN                     m_bGpCopyProtect = FALSE;   // render: konten DRM CopyProtect tidak diteruskan
    // Sisa frame parsial agar aliran byte DMA tetap bersambung dan selaras frame:
    //  render : m_ulGpCarryBytes byte awal frame yang belum lengkap (belum ditulis ke ring).
    //  capture: frame yang sudah dicampur di m_GpCarry; byte [m_ulGpCarryOffset, +m_ulGpCarryBytes)
    //           belum ditulis ke buffer DMA.
    BYTE                        m_GpCarry[GP_MIX_CHANNELS * sizeof(LONG)] = {};
    ULONG                       m_ulGpCarryBytes = 0;
    ULONG                       m_ulGpCarryOffset = 0;
    LONG*                       m_plGpAccumulator = NULL;   // capture: LONG[GP_MIX_CHUNK_FRAMES * GP_MIX_CHANNELS]
    BYTE*                       m_pGpScratch = NULL;    // capture: GP_MIX_SCRATCH_BYTES_PER_CABLE per cable

public:

    //presentation
    NTSTATUS GetPresentationPosition
    (
        _Out_  KSAUDIO_PRESENTATION_POSITION *_pPresentationPosition
    );
        
    ULONG GetCurrentWaveRTWritePosition() 
    {
        return m_ulCurrentWritePosition;
    };

    // To support simple underrun validation.
    BOOL IsCurrentWaveRTWritePositionUpdated() 
    {
        return InterlockedExchange(&m_IsCurrentWritePositionUpdated, 0) ? TRUE : FALSE;
    };

    GUID GetSignalProcessingMode()
    {
        return m_SignalProcessingMode;
    }
    
private:

    //
    // Helper functions.
    //
    
#pragma code_seg()

    VOID WriteBytes
    (
        _In_ ULONG ByteDisplacement
    );
        
    VOID ReadBytes
    (
        _In_ ULONG ByteDisplacement
    );

    VOID GpMixFrames
    (
        _In_ ULONG Frames,
        _Out_writes_bytes_(Frames * m_ulGpFrameBytes) BYTE* Destination
    );

    VOID GpCopyToDma
    (
        _In_ ULONG Offset,
        _In_reads_bytes_(Count) const BYTE* Source,
        _In_ ULONG Count
    );

    VOID GpCopyFromDma
    (
        _Out_writes_bytes_(Count) BYTE* Destination,
        _In_ ULONG Offset,
        _In_ ULONG Count
    );
    
    VOID UpdatePosition
    (
        _In_ LARGE_INTEGER ilQPC
    );
    
    NTSTATUS SetCurrentWritePositionInternal
    (
        _In_  ULONG ulCurrentWritePosition
    );
    
    NTSTATUS GetPositions
    (
        _Out_opt_  ULONGLONG *      _pullLinearBufferPosition, 
        _Out_opt_  ULONGLONG *      _pullPresentationPosition, 
        _Out_opt_  LARGE_INTEGER *  _pliQPCTime
    );

};
typedef CMiniportWaveRTStream *PCMiniportWaveRTStream;
#endif // _SIMPLEAUDIOSAMPLE_MINWAVERTSTREAM_H_

