// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include <windows.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DEFINE_PROVIDER(g_hProvider, "ObsBackgroundRemoval.DirectML",
			     (0x57797101, 0x3691, 0x5b3e, 0xe0, 0xd4, 0xf6, 0x43, 0xa2, 0x08, 0x28, 0x73));
