/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rssfree.c - Free FeedChannel / FeedItem trees
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "rss.h"

void FreeFeedChannel(struct FeedChannel *channel)
{
    struct FeedItem *item;
    struct FeedItem *next;

    if (channel == NULL) {
        return;
    }

    if (channel->title != NULL) {
        FreeVec(channel->title);
    }
    if (channel->link != NULL) {
        FreeVec(channel->link);
    }
    if (channel->description != NULL) {
        FreeVec(channel->description);
    }
    if (channel->language != NULL) {
        FreeVec(channel->language);
    }
    if (channel->copyright != NULL) {
        FreeVec(channel->copyright);
    }
    if (channel->managingeditor != NULL) {
        FreeVec(channel->managingeditor);
    }
    if (channel->webmaster != NULL) {
        FreeVec(channel->webmaster);
    }

    item = channel->items;
    while (item != NULL) {
        next = item->next;
        if (item->title != NULL) {
            FreeVec(item->title);
        }
        if (item->link != NULL) {
            FreeVec(item->link);
        }
        if (item->description != NULL) {
            FreeVec(item->description);
        }
        if (item->pubdate != NULL) {
            FreeVec(item->pubdate);
        }
        if (item->author != NULL) {
            FreeVec(item->author);
        }
        if (item->guid != NULL) {
            FreeVec(item->guid);
        }
        FreeVec(item);
        item = next;
    }

    FreeVec(channel);
}
