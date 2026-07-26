/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rss.h - Standalone RSS/Atom parser (adapted from AWebRssAPL)
 */

#ifndef AMIGAMI_RSS_H
#define AMIGAMI_RSS_H

#include <exec/types.h>

enum FeedType
{
    FEED_UNKNOWN = 0,
    FEED_RSS,
    FEED_ATOM
};

struct FeedItem
{
    struct FeedItem *next;
    UBYTE *title;
    UBYTE *link;
    UBYTE *description;
    UBYTE *pubdate;
    UBYTE *author;
    UBYTE *guid;
    long titlelen;
    long linklen;
    long desclen;
    long pubdatelen;
    long authorlen;
    long guidlen;
    BOOL fi_Read; /* TRUE after user opened the item in preview */
};

struct FeedChannel
{
    UBYTE *title;
    UBYTE *link;
    UBYTE *description;
    UBYTE *language;
    UBYTE *copyright;
    UBYTE *managingeditor;
    UBYTE *webmaster;
    long titlelen;
    long linklen;
    long desclen;
    long languagelen;
    long copyrightlen;
    long managingeditorlen;
    long webmasterlen;
    struct FeedItem *items;
    struct FeedItem *lastitem;
    long itemcount;
    enum FeedType feedtype;
};

struct RssParser
{
    UBYTE *buffer;
    long bufsize;
    long buflen;
    UBYTE *current;
    UBYTE *end;
    struct FeedChannel *channel;
    struct FeedItem *currentitem;
    BOOL initem;
    BOOL incontent;
    UBYTE *currenttag;
    long currenttaglen;
    UBYTE *currentdata;
    long currentdatalen;
    enum FeedType detectedtype;
};

void InitRssParser(struct RssParser *parser);
void ParseRssChunk(struct RssParser *parser, UBYTE *data, long length);
void CleanupRssParser(struct RssParser *parser);
void FreeFeedChannel(struct FeedChannel *channel);

/*
 * Parse a complete buffer. On success returns an owned FeedChannel
 * (caller FreeFeedChannel). On failure returns NULL.
 */
struct FeedChannel *ParseRssBuffer(UBYTE *data, long length);

/* Detach channel from parser so CleanupRssParser does not free it. */
struct FeedChannel *RssParserTakeChannel(struct RssParser *parser);

#endif /* AMIGAMI_RSS_H */
