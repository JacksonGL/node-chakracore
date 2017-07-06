//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

#ifndef TT_MEM_ANALYSIS

#define TT_MEM_ANALYSIS 1
namespace TTD
{
    class TTMemAnalysis {
    public:
        static SnapShot* recentSnapShot;
        static bool dump_prop_JSON;
    };
}
#endif // TT_MEM_ANALYSIS
#endif // ENABLE_TTD