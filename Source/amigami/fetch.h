/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * fetch.h - amihttp Tier 2 GET into a growable buffer
 */

#ifndef AMIGAMI_FETCH_H
#define AMIGAMI_FETCH_H

#include <exec/types.h>
#include <libraries/amihttp.h>

#define AMFETCH_MAX_BODY  (2UL * 1024UL * 1024UL)

struct AmFetchSession
{
    struct HttpSession *afs_Session;
    UBYTE               afs_CaFile[256];
    BOOL                afs_Insecure;
    BOOL                afs_Verbose;
};

/* Open amihttp session (caller already OpenLibrary'd amihttp). */
BOOL AmFetchInit(struct AmFetchSession *fs, STRPTR cafile, BOOL insecure,
    BOOL verbose);
void AmFetchShutdown(struct AmFetchSession *fs);

/*
 * GET url. On success *outBody is AllocVec NUL-terminated body (caller FreeVec),
 * *outLen is byte length without NUL. Returns TRUE on HTTP 2xx with body.
 */
BOOL AmFetchUrl(struct AmFetchSession *fs, STRPTR url,
    UBYTE **outBody, ULONG *outLen, LONG *outHttpStatus, STRPTR errBuf,
    ULONG errMax);

#endif /* AMIGAMI_FETCH_H */
