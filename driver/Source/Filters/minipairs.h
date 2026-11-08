/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minipairs.h

Abstract:

    Local audio endpoint filter definitions.

    GP Virtual Cable: tabel wave ada di cablewavtable.h; pasangan miniport (12 render + 1 capture),
    g_RenderEndpoints, g_CaptureEndpoints dan g_GpCableRoutes dihasilkan dari cables.toml ke
    cables_generated.h (python -m gpcable.codegen).
--*/

#ifndef _SIMPLEAUDIOSAMPLE_MINIPAIRS_H_
#define _SIMPLEAUDIOSAMPLE_MINIPAIRS_H_

#include "speakertopo.h"
#include "speakertoptable.h"

#include "micarraytopo.h"
#include "micarray1toptable.h"

#include "cabletoptable.h"
#include "cablewavtable.h"


NTSTATUS
CreateMiniportWaveRTSimpleAudioSample
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

NTSTATUS
CreateMiniportTopologySimpleAudioSample
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

//
// Render & capture miniport pairs, g_RenderEndpoints[], g_CaptureEndpoints[], g_GpCableRoutes[].
//
#include "cables_generated.h"

C_ASSERT(GP_CAPTURE_CHANNELS == CABLE_CAPTURE_CHANNELS);

#define g_cRenderEndpoints  (SIZEOF_ARRAY(g_RenderEndpoints))

#define g_cCaptureEndpoints (SIZEOF_ARRAY(g_CaptureEndpoints))

C_ASSERT(SIZEOF_ARRAY(g_RenderEndpoints) == GP_CABLE_COUNT);
C_ASSERT(SIZEOF_ARRAY(g_GpCableRoutes) == GP_CABLE_COUNT);

//=============================================================================
//
// Total miniports = # endpoints * 2 (topology + wave).
//
#define g_MaxMiniports  ((g_cRenderEndpoints + g_cCaptureEndpoints) * 2)

#endif // _SIMPLEAUDIOSAMPLE_MINIPAIRS_H_
