/*++

Module Name:

    cablewavtable.h

Abstract:

    GP Virtual Cable: tabel wave miniport untuk 12 endpoint Playback (render) dan satu endpoint
    Recording (capture) 8 channel, plus tabel koneksi fisik topology/wave.

    Diturunkan dari speakerwavtable.h, micarraywavtable.h dan minipairs.h pada sampel Microsoft
    audio/simpleaudiosample (MS-PL).

    Render : 48000 Hz, 24-bit packed, 1 channel (mask mono) atau 2 channel (mask stereo); satu format.
    Capture: 48000 Hz, 8 channel (mask 7.1 surround), 16-bit, 24-bit packed, 32-bit container/24 valid.
--*/

#ifndef _GP_CABLEWAVTABLE_H_
#define _GP_CABLEWAVTABLE_H_

#define CABLE_SAMPLE_RATE                   48000
#define CABLE_RENDER_BITS_PER_SAMPLE        24
#define CABLE_CAPTURE_CHANNELS              8

// Max # of pin instances.
#define CABLE_RENDER_MAX_INPUT_STREAMS      1
#define CABLE_CAPTURE_MAX_OUTPUT_STREAMS    1

// Format default capture (indeks ke CableCaptureDeviceFormats): 24-bit packed.
#define CABLE_CAPTURE_DEFAULT_FORMAT_INDEX  1

//=============================================================================
// Render: format perangkat
//=============================================================================
static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableRenderDeviceFormatsMono[] =
{
    {
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                1,                                  // nChannels
                CABLE_SAMPLE_RATE,                  // nSamplesPerSec
                CABLE_SAMPLE_RATE * 3 * 1,          // nAvgBytesPerSec
                3 * 1,                              // nBlockAlign
                CABLE_RENDER_BITS_PER_SAMPLE,       // wBitsPerSample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            CABLE_RENDER_BITS_PER_SAMPLE,           // wValidBitsPerSample
            KSAUDIO_SPEAKER_MONO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableRenderDeviceFormatsStereo[] =
{
    {
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,                                  // nChannels
                CABLE_SAMPLE_RATE,                  // nSamplesPerSec
                CABLE_SAMPLE_RATE * 3 * 2,          // nAvgBytesPerSec
                3 * 2,                              // nBlockAlign
                CABLE_RENDER_BITS_PER_SAMPLE,       // wBitsPerSample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            CABLE_RENDER_BITS_PER_SAMPLE,           // wValidBitsPerSample
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

//
// Supported modes (only on streaming pins).
//
static
MODE_AND_DEFAULT_FORMAT CableRenderModesMono[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &CableRenderDeviceFormatsMono[0].DataFormat
    }
};

static
MODE_AND_DEFAULT_FORMAT CableRenderModesStereo[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &CableRenderDeviceFormatsStereo[0].DataFormat
    }
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array (KSPIN_WAVE_RENDER3_SINK_SYSTEM, KSPIN_WAVE_RENDER3_SOURCE).
//
static
PIN_DEVICE_FORMATS_AND_MODES CableRenderFormatsMono[] =
{
    {
        SystemRenderPin,
        CableRenderDeviceFormatsMono,
        SIZEOF_ARRAY(CableRenderDeviceFormatsMono),
        CableRenderModesMono,
        SIZEOF_ARRAY(CableRenderModesMono)
    },
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    }
};

static
PIN_DEVICE_FORMATS_AND_MODES CableRenderFormatsStereo[] =
{
    {
        SystemRenderPin,
        CableRenderDeviceFormatsStereo,
        SIZEOF_ARRAY(CableRenderDeviceFormatsStereo),
        CableRenderModesStereo,
        SIZEOF_ARRAY(CableRenderModesStereo)
    },
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    }
};

//=============================================================================
// Render: data range
//=============================================================================
static
KSDATARANGE_AUDIO CableRenderDataRangesMono[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        1,                                  // MaximumChannels
        CABLE_RENDER_BITS_PER_SAMPLE,       // MinimumBitsPerSample
        CABLE_RENDER_BITS_PER_SAMPLE,       // MaximumBitsPerSample
        CABLE_SAMPLE_RATE,                  // MinimumSampleFrequency
        CABLE_SAMPLE_RATE                   // MaximumSampleFrequency
    }
};

static
KSDATARANGE_AUDIO CableRenderDataRangesStereo[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,                                  // MaximumChannels
        CABLE_RENDER_BITS_PER_SAMPLE,       // MinimumBitsPerSample
        CABLE_RENDER_BITS_PER_SAMPLE,       // MaximumBitsPerSample
        CABLE_SAMPLE_RATE,                  // MinimumSampleFrequency
        CABLE_SAMPLE_RATE                   // MaximumSampleFrequency
    }
};

static
PKSDATARANGE CableRenderDataRangePointersMono[] =
{
    PKSDATARANGE(&CableRenderDataRangesMono[0]),
    PKSDATARANGE(&PinDataRangeAttributeList),
};

static
PKSDATARANGE CableRenderDataRangePointersStereo[] =
{
    PKSDATARANGE(&CableRenderDataRangesStereo[0]),
    PKSDATARANGE(&PinDataRangeAttributeList),
};

//=============================================================================
// Bridge data range (render dan capture)
//=============================================================================
static
KSDATARANGE CablePinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE CablePinDataRangePointersBridge[] =
{
    &CablePinDataRangesBridge[0]
};

//=============================================================================
// Render: pin, koneksi, properti, descriptor filter
//=============================================================================
static
PCPIN_DESCRIPTOR CableRenderWavePinsMono[] =
{
    // Wave Out Streaming Pin (Renderer) KSPIN_WAVE_RENDER3_SINK_SYSTEM
    {
        CABLE_RENDER_MAX_INPUT_STREAMS,
        CABLE_RENDER_MAX_INPUT_STREAMS,
        0,
        NULL,        // AutomationTable
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CableRenderDataRangePointersMono),
            CableRenderDataRangePointersMono,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Bridge Pin (Renderer) KSPIN_WAVE_RENDER3_SOURCE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
};

static
PCPIN_DESCRIPTOR CableRenderWavePinsStereo[] =
{
    // Wave Out Streaming Pin (Renderer) KSPIN_WAVE_RENDER3_SINK_SYSTEM
    {
        CABLE_RENDER_MAX_INPUT_STREAMS,
        CABLE_RENDER_MAX_INPUT_STREAMS,
        0,
        NULL,        // AutomationTable
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CableRenderDataRangePointersStereo),
            CableRenderDataRangePointersStereo,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Bridge Pin (Renderer) KSPIN_WAVE_RENDER3_SOURCE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
};

//
//                   ----------------------------
//                   |                          |
//  Host Pin     0-->|                          |--> 1 KSPIN_WAVE_RENDER3_SOURCE
//                   |                          |
//                   ----------------------------
static
PCCONNECTION_DESCRIPTOR CableRenderWaveConnections[] =
{
    { PCFILTER_NODE,            KSPIN_WAVE_RENDER3_SINK_SYSTEM,     PCFILTER_NODE,   KSPIN_WAVE_RENDER3_SOURCE }
};

static
PCPROPERTY_ITEM PropertiesCableRenderWaveFilter[] =
{
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    },
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableRenderWaveFilter, PropertiesCableRenderWaveFilter);

static
PCFILTER_DESCRIPTOR CableRenderWaveDescriptorMono =
{
    0,                                              // Version
    &AutomationCableRenderWaveFilter,               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(CableRenderWavePinsMono),          // PinCount
    CableRenderWavePinsMono,                        // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    0,                                              // NodeCount
    NULL,                                           // Nodes
    SIZEOF_ARRAY(CableRenderWaveConnections),       // ConnectionCount
    CableRenderWaveConnections,                     // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

static
PCFILTER_DESCRIPTOR CableRenderWaveDescriptorStereo =
{
    0,                                              // Version
    &AutomationCableRenderWaveFilter,               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(CableRenderWavePinsStereo),        // PinCount
    CableRenderWavePinsStereo,                      // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    0,                                              // NodeCount
    NULL,                                           // Nodes
    SIZEOF_ARRAY(CableRenderWaveConnections),       // ConnectionCount
    CableRenderWaveConnections,                     // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

//=============================================================================
// Capture: format perangkat (48 kHz, 8 ch, mask 7.1 surround)
//=============================================================================
#define CABLE_CAPTURE_FORMAT(bits, validBits)                                   \
    {                                                                           \
        {                                                                       \
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),                          \
            0,                                                                  \
            0,                                                                  \
            0,                                                                  \
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),                              \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),                             \
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)                   \
        },                                                                      \
        {                                                                       \
            {                                                                   \
                WAVE_FORMAT_EXTENSIBLE,                                         \
                CABLE_CAPTURE_CHANNELS,                                         \
                CABLE_SAMPLE_RATE,                                              \
                CABLE_SAMPLE_RATE * CABLE_CAPTURE_CHANNELS * ((bits) / 8),      \
                CABLE_CAPTURE_CHANNELS * ((bits) / 8),                          \
                (bits),                                                         \
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)             \
            },                                                                  \
            (validBits),                                                        \
            KSAUDIO_SPEAKER_7POINT1_SURROUND,                                   \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)                              \
        }                                                                       \
    }

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableCaptureDeviceFormats[] =
{
    CABLE_CAPTURE_FORMAT(16, 16),   // 0: 16-bit
    CABLE_CAPTURE_FORMAT(24, 24),   // 1: 24-bit packed (default)
    CABLE_CAPTURE_FORMAT(32, 24),   // 2: 32-bit container, 24 valid bits
};

#undef CABLE_CAPTURE_FORMAT

static
MODE_AND_DEFAULT_FORMAT CableCaptureModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &CableCaptureDeviceFormats[CABLE_CAPTURE_DEFAULT_FORMAT_INDEX].DataFormat
    }
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array (KSPIN_WAVE_BRIDGE, KSPIN_WAVEIN_HOST).
//
static
PIN_DEVICE_FORMATS_AND_MODES CableCaptureFormats[] =
{
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    },
    {
        SystemCapturePin,
        CableCaptureDeviceFormats,
        SIZEOF_ARRAY(CableCaptureDeviceFormats),
        CableCaptureModes,
        SIZEOF_ARRAY(CableCaptureModes)
    }
};

//=============================================================================
// Capture: data range. Satu range per lebar sampel; lihat CMiniportWaveRT::DataRangeIntersection.
//=============================================================================
static
KSDATARANGE_AUDIO CableCaptureDataRanges[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_CAPTURE_CHANNELS,
        16,
        16,
        CABLE_SAMPLE_RATE,
        CABLE_SAMPLE_RATE
    },
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_CAPTURE_CHANNELS,
        24,
        24,
        CABLE_SAMPLE_RATE,
        CABLE_SAMPLE_RATE
    },
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_CAPTURE_CHANNELS,
        32,
        32,
        CABLE_SAMPLE_RATE,
        CABLE_SAMPLE_RATE
    },
};

static
PKSDATARANGE CableCaptureDataRangePointers[] =
{
    // All supported device formats should be listed in the DataRange.
    // Urutan: 24-bit (format default) dulu, lalu 32-bit/24 valid, lalu 16-bit, supaya intersection
    // dengan range klien wildcard memilih format default.
    PKSDATARANGE(&CableCaptureDataRanges[1]),
    PKSDATARANGE(&PinDataRangeAttributeList),
    PKSDATARANGE(&CableCaptureDataRanges[2]),
    PKSDATARANGE(&PinDataRangeAttributeList),
    PKSDATARANGE(&CableCaptureDataRanges[0]),
    PKSDATARANGE(&PinDataRangeAttributeList),
};

//=============================================================================
// Capture: pin, node, koneksi, properti, descriptor filter
//=============================================================================
static
PCPIN_DESCRIPTOR CableCaptureWavePins[] =
{
    // Wave In Bridge Pin (Capture - From Topology) KSPIN_WAVE_BRIDGE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave In Streaming Pin (Capture) KSPIN_WAVEIN_HOST
    {
        CABLE_CAPTURE_MAX_OUTPUT_STREAMS,
        CABLE_CAPTURE_MAX_OUTPUT_STREAMS,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CableCaptureDataRangePointers),
            CableCaptureDataRangePointers,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_RECORDING_CONTROL,
            0
        }
    }
};

static
PCNODE_DESCRIPTOR CableCaptureWaveNodes[] =
{
    // KSNODE_WAVE_ADC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    }
};

static
PCCONNECTION_DESCRIPTOR CableCaptureWaveConnections[] =
{
    { PCFILTER_NODE,        KSPIN_WAVE_BRIDGE,      KSNODE_WAVE_ADC,     1 },
    { KSNODE_WAVE_ADC,      0,                      PCFILTER_NODE,       KSPIN_WAVEIN_HOST },
};

static
SIMPLEAUDIOSAMPLE_PROPERTY_ITEM PropertiesCableCaptureWaveFilter[] =
{
    {
        {
            &KSPROPSETID_General,
            KSPROPERTY_GENERAL_COMPONENTID,
            KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        0
    },
    {
        {
            &KSPROPSETID_Pin,
            KSPROPERTY_PIN_PROPOSEDATAFORMAT,
            KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        0
    },
    {
        {
            &KSPROPSETID_Pin,
            KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
            KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        0
    },
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableCaptureWaveFilter, PropertiesCableCaptureWaveFilter);

static
PCFILTER_DESCRIPTOR CableCaptureWaveDescriptor =
{
    0,                                              // Version
    &AutomationCableCaptureWaveFilter,              // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(CableCaptureWavePins),             // PinCount
    CableCaptureWavePins,                           // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    SIZEOF_ARRAY(CableCaptureWaveNodes),            // NodeCount
    CableCaptureWaveNodes,                          // Nodes
    SIZEOF_ARRAY(CableCaptureWaveConnections),      // ConnectionCount
    CableCaptureWaveConnections,                    // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

//=============================================================================
// Koneksi fisik topology/wave (dipindah dari minipairs.h sampel).
//=============================================================================

/*********************************************************************
* Topology/Wave bridge connection for each render cable              *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Line Out         *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE SpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
        KSPIN_WAVE_RENDER3_SOURCE,  // WaveOut
        CONNECTIONTYPE_WAVE_OUTPUT
    }
};

/*********************************************************************
* Topology/Wave bridge connection for the capture endpoint           *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
*  Mic in  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE MicArray1TopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,          // TopologyOut
        KSPIN_WAVE_BRIDGE,          // WaveIn
        CONNECTIONTYPE_TOPOLOGY_OUTPUT
    }
};

#endif // _GP_CABLEWAVTABLE_H_
