/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * feedstore.h - Subscription list + OPML persistence
 *
 * Canonical store: ENVARC:Amigami/feeds.opml
 * Legacy import:   PROGDIR:Amigami.prefs (URL "Title" lines)
 */

#ifndef AMIGAMI_FEEDSTORE_H
#define AMIGAMI_FEEDSTORE_H

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>

#include "rss.h"

#define AMFEED_URL_MAX    512
#define AMFEED_TITLE_MAX  256
#define AMFEED_ERR_MAX    128

#define AMFEED_OPML_DEFAULT "ENVARC:Amigami/feeds.opml"
#define AMFEED_PREFS_LEGACY "PROGDIR:Amigami.prefs"

struct AmFeed
{
    struct Node         af_Node;       /* ln_Name points at af_Title */
    UBYTE               af_Url[AMFEED_URL_MAX];
    UBYTE               af_Title[AMFEED_TITLE_MAX];
    UBYTE               af_LastError[AMFEED_ERR_MAX];
    struct FeedChannel *af_Channel;    /* owned cache; may be NULL */
};

struct AmFeedStore
{
    struct List         fs_Feeds;
    ULONG               fs_Count;
    UBYTE               fs_OpmlPath[256];
    UBYTE               fs_LegacyPrefs[256];
};

void AmFeedStoreInit(struct AmFeedStore *store);
void AmFeedStoreFree(struct AmFeedStore *store);

/* Resolve OPML path (override or ENVARC:Amigami/feeds.opml). */
BOOL AmFeedStoreSetPaths(struct AmFeedStore *store, STRPTR opmlOverride);

BOOL AmFeedStoreLoad(struct AmFeedStore *store);
BOOL AmFeedStoreSave(struct AmFeedStore *store);

struct AmFeed *AmFeedStoreAdd(struct AmFeedStore *store,
    STRPTR url, STRPTR title);
BOOL AmFeedStoreRemove(struct AmFeedStore *store, struct AmFeed *feed);

void AmFeedSetChannel(struct AmFeed *feed, struct FeedChannel *ch);
void AmFeedClearChannel(struct AmFeed *feed);

struct AmFeed *AmFeedStoreFindByUrl(struct AmFeedStore *store, STRPTR url);

/* Back-compat alias used by older call sites. */
#define AmFeedStoreSetPrefsPath(store, path) AmFeedStoreSetPaths((store), (path))

#endif /* AMIGAMI_FEEDSTORE_H */
