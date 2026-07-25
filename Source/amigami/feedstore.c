/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * feedstore.c - Subscriptions in ENVARC:Amigami/feeds.opml (OPML 1.0)
 *
 * On first run, imports PROGDIR:Amigami.prefs if present, then writes OPML.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>
#include <string.h>
#include <stdio.h>

#include "feedstore.h"
#include "utf8fold.h"
#include "amlog.h"

#ifndef ZERO
#define ZERO ((BPTR)0)
#endif

static void
feed_free(struct AmFeed *feed)
{
    if (feed == NULL) {
        return;
    }
    AmFeedClearChannel(feed);
    FreeVec(feed);
}

static BOOL
ensure_amigami_dir(void)
{
    BPTR lock;

    lock = Lock((STRPTR)"ENVARC:Amigami", ACCESS_READ);
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    lock = CreateDir((STRPTR)"ENVARC:Amigami");
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    /* CreateDir may fail if exists on some ROMs - try Lock again */
    lock = Lock((STRPTR)"ENVARC:Amigami", ACCESS_READ);
    if (lock != ZERO) {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

void
AmFeedStoreInit(struct AmFeedStore *store)
{
    if (store == NULL) {
        return;
    }
    memset(store, 0, sizeof(*store));
    NewList(&store->fs_Feeds);
    strcpy((char *)store->fs_OpmlPath, AMFEED_OPML_DEFAULT);
    strcpy((char *)store->fs_LegacyPrefs, AMFEED_PREFS_LEGACY);
}

void
AmFeedStoreFree(struct AmFeedStore *store)
{
    struct AmFeed *feed;
    struct AmFeed *next;

    if (store == NULL) {
        return;
    }
    feed = (struct AmFeed *)store->fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        next = (struct AmFeed *)feed->af_Node.ln_Succ;
        Remove(&feed->af_Node);
        feed_free(feed);
        feed = next;
    }
    NewList(&store->fs_Feeds);
    store->fs_Count = 0;
}

BOOL
AmFeedStoreSetPaths(struct AmFeedStore *store, STRPTR opmlOverride)
{
    if (store == NULL) {
        return FALSE;
    }
    if (opmlOverride != NULL && opmlOverride[0] != '\0') {
        strncpy((char *)store->fs_OpmlPath, (char *)opmlOverride,
            sizeof(store->fs_OpmlPath) - 1);
        store->fs_OpmlPath[sizeof(store->fs_OpmlPath) - 1] = '\0';
    } else {
        strcpy((char *)store->fs_OpmlPath, AMFEED_OPML_DEFAULT);
    }
    strcpy((char *)store->fs_LegacyPrefs, AMFEED_PREFS_LEGACY);
    return TRUE;
}

void
AmFeedClearChannel(struct AmFeed *feed)
{
    if (feed == NULL) {
        return;
    }
    if (feed->af_Channel != NULL) {
        FreeFeedChannel(feed->af_Channel);
        feed->af_Channel = NULL;
    }
}

void
AmFeedSetChannel(struct AmFeed *feed, struct FeedChannel *ch)
{
    BOOL keepTitle;

    if (feed == NULL) {
        return;
    }
    AmFeedClearChannel(feed);
    feed->af_Channel = ch;

    keepTitle = FALSE;
    if (feed->af_Title[0] != '\0' &&
        strnicmp((char *)feed->af_Title, "http://", 7) != 0 &&
        strnicmp((char *)feed->af_Title, "https://", 8) != 0 &&
        stricmp((char *)feed->af_Title, (char *)feed->af_Url) != 0) {
        keepTitle = TRUE;
    }

    if (!keepTitle && ch != NULL && ch->title != NULL && ch->title[0] != '\0') {
        AmLatin1Copy(feed->af_Title, sizeof(feed->af_Title),
            (STRPTR)ch->title);
    } else {
        AmCleanupLatin1Field(feed->af_Title);
    }
    feed->af_Node.ln_Name = (char *)feed->af_Title;
    AmLogStringCheck((STRPTR)"feed.af_Title", (STRPTR)feed->af_Title);
}

struct AmFeed *
AmFeedStoreFindByUrl(struct AmFeedStore *store, STRPTR url)
{
    struct AmFeed *feed;

    if (store == NULL || url == NULL) {
        return NULL;
    }
    feed = (struct AmFeed *)store->fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        if (stricmp((char *)feed->af_Url, (char *)url) == 0) {
            return feed;
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }
    return NULL;
}

struct AmFeed *
AmFeedStoreAdd(struct AmFeedStore *store, STRPTR url, STRPTR title)
{
    struct AmFeed *feed;

    if (store == NULL || url == NULL || url[0] == '\0') {
        return NULL;
    }
    if (AmFeedStoreFindByUrl(store, url) != NULL) {
        return NULL;
    }

    feed = (struct AmFeed *)AllocVec(sizeof(struct AmFeed), MEMF_CLEAR);
    if (feed == NULL) {
        return NULL;
    }

    strncpy((char *)feed->af_Url, (char *)url, sizeof(feed->af_Url) - 1);
    if (title != NULL && title[0] != '\0') {
        AmLatin1Copy(feed->af_Title, sizeof(feed->af_Title), title);
    } else {
        AmLatin1Copy(feed->af_Title, sizeof(feed->af_Title), url);
    }
    feed->af_Node.ln_Name = (char *)feed->af_Title;
    feed->af_Node.ln_Type = NT_USER;
    feed->af_Node.ln_Pri = 0;

    AddTail(&store->fs_Feeds, &feed->af_Node);
    store->fs_Count++;
    return feed;
}

BOOL
AmFeedStoreRemove(struct AmFeedStore *store, struct AmFeed *feed)
{
    if (store == NULL || feed == NULL) {
        return FALSE;
    }
    Remove(&feed->af_Node);
    if (store->fs_Count > 0) {
        store->fs_Count--;
    }
    feed_free(feed);
    return TRUE;
}

/* --- legacy prefs --- */

static BOOL
parse_prefs_line(UBYTE *line, UBYTE *urlOut, ULONG urlMax,
    UBYTE *titleOut, ULONG titleMax)
{
    UBYTE *p;
    UBYTE *u;
    ULONG n;

    if (line == NULL || urlOut == NULL) {
        return FALSE;
    }
    urlOut[0] = '\0';
    if (titleOut != NULL && titleMax > 0) {
        titleOut[0] = '\0';
    }

    p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '#' || *p == ';') {
        return FALSE;
    }

    u = urlOut;
    n = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && n + 1 < urlMax) {
        *u++ = *p++;
        n++;
    }
    *u = '\0';
    if (n == 0) {
        return FALSE;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (titleOut == NULL || titleMax == 0 || *p == '\0') {
        return TRUE;
    }

    if (*p == '"') {
        p++;
        u = titleOut;
        n = 0;
        while (*p != '\0' && *p != '"' && n + 1 < titleMax) {
            *u++ = *p++;
            n++;
        }
        *u = '\0';
    } else {
        strncpy((char *)titleOut, (char *)p, titleMax - 1);
        titleOut[titleMax - 1] = '\0';
    }
    return TRUE;
}

static BOOL
load_legacy_prefs(struct AmFeedStore *store)
{
    BPTR fh;
    UBYTE line[AMFEED_URL_MAX + AMFEED_TITLE_MAX + 16];
    UBYTE url[AMFEED_URL_MAX];
    UBYTE title[AMFEED_TITLE_MAX];
    ULONG before;

    fh = Open(store->fs_LegacyPrefs, MODE_OLDFILE);
    if (fh == ZERO) {
        return FALSE;
    }
    before = store->fs_Count;
    while (FGets(fh, (STRPTR)line, sizeof(line)) != NULL) {
        ULONG len;

        len = strlen((char *)line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (parse_prefs_line(line, url, sizeof(url), title, sizeof(title))) {
            (void)AmFeedStoreAdd(store, url,
                title[0] != '\0' ? title : NULL);
        }
    }
    Close(fh);
    if (store->fs_Count > before) {
        AmLog("Amigami: imported %ld feeds from %s\n",
            (long)(store->fs_Count - before), store->fs_LegacyPrefs);
        return TRUE;
    }
    return FALSE;
}

/* --- OPML --- */

static UBYTE *
attr_get(UBYTE *attrs, UBYTE *end, STRPTR name, long *outLen)
{
    long nlen;
    UBYTE *p;
    UBYTE quote;

    if (attrs == NULL || end == NULL || name == NULL) {
        return NULL;
    }
    nlen = (long)strlen((char *)name);
    p = attrs;
    while (p + nlen < end) {
        if (strnicmp(p, name, nlen) == 0 &&
            (p == attrs || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n')) {
            p += nlen;
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++;
            }
            if (p < end && *p == '=') {
                p++;
                while (p < end && (*p == ' ' || *p == '\t')) {
                    p++;
                }
                if (p < end && (*p == '"' || *p == '\'')) {
                    quote = *p++;
                    attrs = p;
                    while (p < end && *p != quote) {
                        p++;
                    }
                    if (outLen != NULL) {
                        *outLen = p - attrs;
                    }
                    return attrs;
                }
            }
        }
        p++;
    }
    return NULL;
}

static void
copy_attr(UBYTE *dst, ULONG dstmax, UBYTE *src, long len)
{
    ULONG n;
    ULONG i;

    if (dst == NULL || dstmax == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL || len <= 0) {
        return;
    }
    n = (ULONG)len;
    if (n >= dstmax) {
        n = dstmax - 1;
    }
    for (i = 0; i < n; i++) {
        dst[i] = src[i];
    }
    dst[n] = '\0';
    AmCleanupLatin1Field(dst);
}

static BOOL
load_opml(struct AmFeedStore *store)
{
    BPTR fh;
    UBYTE *buf;
    LONG size;
    LONG got;
    UBYTE *p;
    UBYTE *end;
    ULONG before;

    fh = Open(store->fs_OpmlPath, MODE_OLDFILE);
    if (fh == ZERO) {
        return FALSE;
    }
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    if (size <= 0) {
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
    if (got <= 0) {
        FreeVec(buf);
        return FALSE;
    }
    buf[got] = '\0';
    end = buf + got;
    before = store->fs_Count;

    p = buf;
    while (p < end) {
        UBYTE *gt;
        UBYTE *xmlUrl;
        UBYTE *textAttr;
        UBYTE *titleAttr;
        long xmlLen;
        long textLen;
        long titleLen;
        UBYTE url[AMFEED_URL_MAX];
        UBYTE title[AMFEED_TITLE_MAX];

        if (strnicmp(p, "<outline", 8) != 0) {
            p++;
            continue;
        }
        gt = p;
        while (gt < end && *gt != '>') {
            gt++;
        }
        if (gt >= end) {
            break;
        }

        xmlLen = 0;
        textLen = 0;
        titleLen = 0;
        xmlUrl = attr_get(p + 8, gt, (STRPTR)"xmlUrl", &xmlLen);
        if (xmlUrl == NULL) {
            xmlUrl = attr_get(p + 8, gt, (STRPTR)"xmlurl", &xmlLen);
        }
        textAttr = attr_get(p + 8, gt, (STRPTR)"text", &textLen);
        titleAttr = attr_get(p + 8, gt, (STRPTR)"title", &titleLen);

        if (xmlUrl != NULL && xmlLen > 0) {
            copy_attr(url, sizeof(url), xmlUrl, xmlLen);
            title[0] = '\0';
            if (titleAttr != NULL && titleLen > 0) {
                copy_attr(title, sizeof(title), titleAttr, titleLen);
            } else if (textAttr != NULL && textLen > 0) {
                copy_attr(title, sizeof(title), textAttr, textLen);
            }
            if (url[0] != '\0') {
                (void)AmFeedStoreAdd(store, url,
                    title[0] != '\0' ? title : NULL);
            }
        }
        p = gt + 1;
    }

    FreeVec(buf);
    if (store->fs_Count > before) {
        AmLog("Amigami: loaded %ld feeds from %s\n",
            (long)(store->fs_Count - before), store->fs_OpmlPath);
        return TRUE;
    }
    return FALSE;
}

static void
xml_escape_attr(UBYTE *dst, ULONG dstmax, STRPTR src)
{
    ULONG left;
    UBYTE *d;

    if (dst == NULL || dstmax == 0) {
        return;
    }
    d = dst;
    left = dstmax;
    *d = '\0';
    if (src == NULL) {
        return;
    }
    while (*src != '\0' && left > 1) {
        if (*src == '&') {
            if (left <= 5) {
                break;
            }
            strcpy((char *)d, "&amp;");
            d += 5;
            left -= 5;
        } else if (*src == '"') {
            if (left <= 6) {
                break;
            }
            strcpy((char *)d, "&quot;");
            d += 6;
            left -= 6;
        } else if (*src == '<') {
            if (left <= 4) {
                break;
            }
            strcpy((char *)d, "&lt;");
            d += 4;
            left -= 4;
        } else {
            *d++ = (UBYTE)*src;
            left--;
        }
        src++;
    }
    *d = '\0';
}

BOOL
AmFeedStoreSave(struct AmFeedStore *store)
{
    BPTR fh;
    struct AmFeed *feed;
    UBYTE escTitle[AMFEED_TITLE_MAX * 2];
    UBYTE escUrl[AMFEED_URL_MAX * 2];
    UBYTE line[AMFEED_URL_MAX * 2 + AMFEED_TITLE_MAX * 2 + 64];

    if (store == NULL) {
        return FALSE;
    }
    if (!ensure_amigami_dir()) {
        AmLogAlways((STRPTR)"Amigami: cannot create ENVARC:Amigami\n");
        return FALSE;
    }

    fh = Open(store->fs_OpmlPath, MODE_NEWFILE);
    if (fh == ZERO) {
        AmLogAlways((STRPTR)"Amigami: cannot write %s\n", store->fs_OpmlPath);
        return FALSE;
    }

    FPuts(fh, (STRPTR)"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
    FPuts(fh, (STRPTR)"<opml version=\"1.0\">\n");
    FPuts(fh, (STRPTR)"<head><title>Amigami Feeds</title></head>\n");
    FPuts(fh, (STRPTR)"<body>\n");

    feed = (struct AmFeed *)store->fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        xml_escape_attr(escTitle, sizeof(escTitle), (STRPTR)feed->af_Title);
        xml_escape_attr(escUrl, sizeof(escUrl), (STRPTR)feed->af_Url);
        sprintf((char *)line,
            "<outline type=\"rss\" text=\"%s\" title=\"%s\" xmlUrl=\"%s\"/>\n",
            (char *)escTitle, (char *)escTitle, (char *)escUrl);
        FPuts(fh, (STRPTR)line);
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }

    FPuts(fh, (STRPTR)"</body>\n</opml>\n");
    Close(fh);
    AmLog("Amigami: saved %ld feeds to %s\n",
        (long)store->fs_Count, store->fs_OpmlPath);
    return TRUE;
}

BOOL
AmFeedStoreLoad(struct AmFeedStore *store)
{
    BOOL fromOpml;
    BOOL fromPrefs;

    if (store == NULL) {
        return FALSE;
    }

    fromOpml = load_opml(store);
    if (fromOpml) {
        return TRUE;
    }

    fromPrefs = load_legacy_prefs(store);
    if (fromPrefs) {
        (void)AmFeedStoreSave(store);
        return TRUE;
    }
    return TRUE; /* empty store is OK */
}
