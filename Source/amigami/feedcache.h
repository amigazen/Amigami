/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * feedcache.h - T:Amigami disk cache for downloaded feed XML
 */

#ifndef AMIGAMI_FEEDCACHE_H
#define AMIGAMI_FEEDCACHE_H

#include <exec/types.h>

#define AMCACHE_DIR "T:Amigami"

/* Ensure T:Amigami exists. */
BOOL AmFeedCacheInit(void);

/* Build cache path for url into out (e.g. T:Amigami/a1b2c3d4.xml). */
BOOL AmFeedCachePath(STRPTR url, STRPTR out, ULONG outMax);

/* Save raw XML body. */
BOOL AmFeedCacheSave(STRPTR url, UBYTE *body, ULONG len);

/*
 * Load cached body. On success *outBody is AllocVec (caller FreeVec),
 * *outLen set. Returns FALSE if missing.
 */
BOOL AmFeedCacheLoad(STRPTR url, UBYTE **outBody, ULONG *outLen);

BOOL AmFeedCacheDelete(STRPTR url);

#endif /* AMIGAMI_FEEDCACHE_H */
