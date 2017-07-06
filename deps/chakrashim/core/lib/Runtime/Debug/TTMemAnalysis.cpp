//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Debug/TTSnapshot.h"
#include "Debug/TTMEMAnalysis.h"

#if ENABLE_TTD

namespace TTD
{
    SnapShot* TTMemAnalysis::recentSnapShot = nullptr;
    bool TTMemAnalysis::dump_prop_JSON = true;
}

#endif