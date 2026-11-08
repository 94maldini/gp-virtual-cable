/*++

Module Name:

    cabletoptable.h

Abstract:

    GP Virtual Cable: descriptor filter topology per endpoint. Sama dengan tabel topology sampel
    (speakertoptable.h untuk render, micarray1toptable.h untuk capture), kecuali pin bridge endpoint
    yang diberi GUID Name sendiri. KSPROPERTY_PIN_NAME mencari GUID itu di MediaCategories (INX,
    HKR device software key) sehingga nama endpoint di Windows = nama cable dari cables.toml.

    cables_generated.h memanggil GP_DEFINE_RENDER_TOPO / GP_DEFINE_CAPTURE_TOPO satu kali per endpoint.
    Hanya pin descriptor dan descriptor filter yang digandakan; data range, node, koneksi dan tabel
    otomasi tetap milik tabel sampel.
--*/

#ifndef _GP_CABLETOPTABLE_H_
#define _GP_CABLETOPTABLE_H_

//
// Render: KSPIN_TOPO_WAVEOUT_SOURCE (0), KSPIN_TOPO_LINEOUT_DEST (1, bridge, kategori speaker).
// Menghasilkan GpTopoPins<id>[] dan GpTopoDescriptor<id>.
//
#define GP_DEFINE_RENDER_TOPO(id, nameGuid)                                                 \
static                                                                                      \
PCPIN_DESCRIPTOR GpTopoPins##id[] =                                                         \
{                                                                                           \
    /* KSPIN_TOPO_WAVEOUT_SOURCE */                                                         \
    {                                                                                       \
        0, 0, 0, NULL,                                                                      \
        {                                                                                   \
            0, NULL, 0, NULL,                                                               \
            SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),                            \
            SpeakerTopoPinDataRangePointersBridge,                                          \
            KSPIN_DATAFLOW_IN,                                                              \
            KSPIN_COMMUNICATION_NONE,                                                       \
            &KSCATEGORY_AUDIO,                                                              \
            NULL,                                                                           \
            0                                                                               \
        }                                                                                   \
    },                                                                                      \
    /* KSPIN_TOPO_LINEOUT_DEST: bridge pin endpoint, Name = GUID nama cable */              \
    {                                                                                       \
        0, 0, 0, NULL,                                                                      \
        {                                                                                   \
            0, NULL, 0, NULL,                                                               \
            SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),                            \
            SpeakerTopoPinDataRangePointersBridge,                                          \
            KSPIN_DATAFLOW_OUT,                                                             \
            KSPIN_COMMUNICATION_NONE,                                                       \
            &KSNODETYPE_SPEAKER,                                                            \
            &(nameGuid),                                                                    \
            0                                                                               \
        }                                                                                   \
    }                                                                                       \
};                                                                                          \
static                                                                                      \
PCFILTER_DESCRIPTOR GpTopoDescriptor##id =                                                  \
{                                                                                           \
    0,                                              /* Version */                           \
    &AutomationSpeakerTopoFilter,                   /* AutomationTable */                   \
    sizeof(PCPIN_DESCRIPTOR),                       /* PinSize */                           \
    SIZEOF_ARRAY(GpTopoPins##id),                   /* PinCount */                          \
    GpTopoPins##id,                                 /* Pins */                              \
    sizeof(PCNODE_DESCRIPTOR),                      /* NodeSize */                          \
    SIZEOF_ARRAY(SpeakerTopologyNodes),             /* NodeCount */                         \
    SpeakerTopologyNodes,                           /* Nodes */                             \
    SIZEOF_ARRAY(SpeakerTopoMiniportConnections),   /* ConnectionCount */                   \
    SpeakerTopoMiniportConnections,                 /* Connections */                       \
    0,                                              /* CategoryCount */                     \
    NULL                                            /* Categories */                        \
};

//
// Capture: KSPIN_TOPO_MIC_ELEMENTS (0, bridge, kategori mic array), KSPIN_TOPO_BRIDGE (1).
// Menghasilkan GpTopoPins<id>[] dan GpTopoDescriptor<id>.
//
#define GP_DEFINE_CAPTURE_TOPO(id, nameGuid)                                                \
static                                                                                      \
PCPIN_DESCRIPTOR GpTopoPins##id[] =                                                         \
{                                                                                           \
    /* KSPIN_TOPO_MIC_ELEMENTS: bridge pin endpoint, Name = GUID nama Recording */          \
    {                                                                                       \
        0, 0, 0, NULL,                                                                      \
        {                                                                                   \
            0, NULL, 0, NULL,                                                               \
            SIZEOF_ARRAY(MicArray1TopoPinDataRangePointersBridge),                          \
            MicArray1TopoPinDataRangePointersBridge,                                        \
            KSPIN_DATAFLOW_IN,                                                              \
            KSPIN_COMMUNICATION_NONE,                                                       \
            &KSNODETYPE_MICROPHONE_ARRAY,                                                   \
            &(nameGuid),                                                                    \
            0                                                                               \
        }                                                                                   \
    },                                                                                      \
    /* KSPIN_TOPO_BRIDGE */                                                                 \
    {                                                                                       \
        0, 0, 0, NULL,                                                                      \
        {                                                                                   \
            0, NULL, 0, NULL,                                                               \
            SIZEOF_ARRAY(MicArray1TopoPinDataRangePointersBridge),                          \
            MicArray1TopoPinDataRangePointersBridge,                                        \
            KSPIN_DATAFLOW_OUT,                                                             \
            KSPIN_COMMUNICATION_NONE,                                                       \
            &KSCATEGORY_AUDIO,                                                              \
            NULL,                                                                           \
            0                                                                               \
        }                                                                                   \
    }                                                                                       \
};                                                                                          \
static                                                                                      \
PCFILTER_DESCRIPTOR GpTopoDescriptor##id =                                                  \
{                                                                                           \
    0,                                              /* Version */                           \
    &AutomationMicArray1TopoFilter,                 /* AutomationTable */                   \
    sizeof(PCPIN_DESCRIPTOR),                       /* PinSize */                           \
    SIZEOF_ARRAY(GpTopoPins##id),                   /* PinCount */                          \
    GpTopoPins##id,                                 /* Pins */                              \
    sizeof(PCNODE_DESCRIPTOR),                      /* NodeSize */                          \
    SIZEOF_ARRAY(MicArray1TopologyNodes),           /* NodeCount */                         \
    MicArray1TopologyNodes,                         /* Nodes */                             \
    SIZEOF_ARRAY(MicArray1TopoMiniportConnections), /* ConnectionCount */                   \
    MicArray1TopoMiniportConnections,               /* Connections */                       \
    0,                                              /* CategoryCount */                     \
    NULL                                            /* Categories */                        \
};

// Pin descriptor endpoint harus sejajar dengan tabel sampel (indeks pin dipakai handler jack/mic).
C_ASSERT(SIZEOF_ARRAY(SpeakerTopoMiniportPins) == 2);
C_ASSERT(KSPIN_TOPO_LINEOUT_DEST == 1);
C_ASSERT(SIZEOF_ARRAY(MicArray1TopoMiniportPins) == 2);
C_ASSERT(KSPIN_TOPO_MIC_ELEMENTS == 0);

#endif // _GP_CABLETOPTABLE_H_
