/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * feedcache.c - Persist feed XML under T:Amigami/
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>

#include "feedcache.h"

#ifndef ZERO
#define ZERO ((BPTR)0)
#endif

/* Simple FNV-1a 32-bit hash → 8 hex chars for cache filenames. */
static ULONG
hash_url(STRPTR url)
{
    ULONG h;
    UBYTE *p;

    h = 2166136261UL;
    p = (UBYTE *)url;
    if (p == NULL) {
        return h;
    }
    while (*p != '\0') {
        h ^= (ULONG)(*p++);
        h *= 16777619UL;
    }
    return h;
}

BOOL
AmFeedCacheInit(void)
{
    BPTR lock;

    lock = Lock((STRPTR)AMCACHE_DIR, SHARED_LOCK);
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    lock = CreateDir((STRPTR)AMCACHE_DIR);
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    /* CreateDir may return ZERO if exists on some systems - try again */
    lock = Lock((STRPTR)AMCACHE_DIR, SHARED_LOCK);
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

BOOL
AmFeedCachePath(STRPTR url, STRPTR out, ULONG outMax)
{
    ULONG h;

    if (url == NULL || out == NULL || outMax < 24) {
        return FALSE;
    }
    h = hash_url(url);
    sprintf((char *)out, "%s/%08lx.xml", AMCACHE_DIR, (unsigned long)h);
    return TRUE;
}

BOOL
AmFeedCacheSave(STRPTR url, UBYTE *body, ULONG len)
{
    UBYTE path[64];
    BPTR fh;
    LONG written;

    if (body == NULL) {
        return FALSE;
    }
    AmFeedCacheInit();
    if (!AmFeedCachePath(url, path, sizeof(path))) {
        return FALSE;
    }
    fh = Open(path, MODE_NEWFILE);
    if (fh == ZERO) {
        return FALSE;
    }
    written = Write(fh, body, (LONG)len);
    Close(fh);
    if (written == (LONG)len) {
        return TRUE;
    }
    return FALSE;
}

BOOL
AmFeedCacheLoad(STRPTR url, UBYTE **outBody, ULONG *outLen)
{
    UBYTE path[64];
    BPTR fh;
    LONG size;
    UBYTE *buf;
    LONG got;

    if (outBody != NULL) {
        *outBody = NULL;
    }
    if (outLen != NULL) {
        *outLen = 0;
    }
    if (url == NULL || outBody == NULL) {
        return FALSE;
    }
    if (!AmFeedCachePath(url, path, sizeof(path))) {
        return FALSE;
    }
    fh = Open(path, MODE_OLDFILE);
    if (fh == ZERO) {
        return FALSE;
    }
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    if (size <= 0 || size > (LONG)(2L * 1024L * 1024L)) {
        Close(fh);
        return FALSE;
    }
    buf = (UBYTE *)AllocVec((ULONG)size + 1, MEMF_ANY);
    if (buf == NULL) {
        Close(fh);
        return FALSE;
    }
    got = Read(fh, buf, size);
    Close(fh);
    if (got != size) {
        FreeVec(buf);
        return FALSE;
    }
    buf[size] = '\0';
    *outBody = buf;
    if (outLen != NULL) {
        *outLen = (ULONG)size;
    }
    return TRUE;
}

BOOL
AmFeedCacheDelete(STRPTR url)
{
    UBYTE path[64];

    if (!AmFeedCachePath(url, path, sizeof(path))) {
        return FALSE;
    }
    if (DeleteFile(path) != 0) {
        return TRUE;
    }
    return FALSE;
}
