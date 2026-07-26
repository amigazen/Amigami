/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_articles.c - Article listbrowser
 *
 * listbrowser.gadget has no multiline rows. Layout is NetNewsWire-ish:
 *   Title (wide) | Date (compact) - source/author lives in the date column
 *   as "25 Jul - FeedName" when available.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"
#include "utf8fold.h"
#include "datefmt.h"
#include "rtb_article.h"
#include "amlog.h"

BOOL
AmItemIsToday(STRPTR pubdate)
{
    return AmDateIsToday(pubdate);
}

static void
arts_detach(struct AmigamiGui *gui)
{
    if (gui->ag_ArticlesLB != NULL && gui->ag_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->ag_ArticlesLB, gui->ag_Window,
            NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
    } else if (gui->ag_ArticlesLB != NULL) {
        SetAttrs(gui->ag_ArticlesLB, LISTBROWSER_Labels, ~0, TAG_DONE);
    }
}

static void
arts_attach(struct AmigamiGui *gui)
{
    if (gui->ag_ArticlesLB != NULL && gui->ag_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->ag_ArticlesLB, gui->ag_Window,
            NULL, LISTBROWSER_Labels, (ULONG)&gui->ag_ArticleNodes, TAG_DONE);
    } else if (gui->ag_ArticlesLB != NULL) {
        SetAttrs(gui->ag_ArticlesLB,
            LISTBROWSER_Labels, (ULONG)&gui->ag_ArticleNodes, TAG_DONE);
    }
}

BOOL
AmigamiArticlesInit(struct AmigamiGui *gui)
{
    NewList(&gui->ag_ArticleNodes);
    /*
     * Two columns only - listbrowser cannot draw multiline cells.
     * Title is primary; Date holds "25 Jul - source".
     */
    gui->ag_ArticleCI = AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, (ULONG)"Title",
            LBCIA_Weight, 72,
            LBCIA_Flags, CIF_WEIGHTED,
        LBCIA_Column, 1,
            LBCIA_Title, (ULONG)"Date",
            LBCIA_Weight, 28,
            LBCIA_Flags, CIF_WEIGHTED,
        TAG_DONE);
    if (gui->ag_ArticleCI != NULL) {
        return TRUE;
    }
    return FALSE;
}

void
AmigamiArticlesClear(struct AmigamiGui *gui)
{
    arts_detach(gui);
    FreeListBrowserList(&gui->ag_ArticleNodes);
    NewList(&gui->ag_ArticleNodes);
    arts_attach(gui);
    gui->ag_SelectedItem = NULL;
}

void
AmigamiArticlesFree(struct AmigamiGui *gui)
{
    AmigamiArticlesClear(gui);
    if (gui->ag_ArticleCI != NULL) {
        FreeLBColumnInfo(gui->ag_ArticleCI);
        gui->ag_ArticleCI = NULL;
    }
}

static void
add_item_nodes(struct AmigamiGui *gui, struct FeedChannel *ch,
    STRPTR sourceHint)
{
    struct FeedItem *item;
    struct Node *node;
    UBYTE titleFold[280];
    UBYTE meta[96];
    UBYTE shortDate[16];
    UBYTE srcFold[64];

    if (ch == NULL) {
        return;
    }

    arts_detach(gui);

    item = ch->items;
    while (item != NULL) {
        titleFold[0] = '\0';
        meta[0] = '\0';
        shortDate[0] = '\0';
        srcFold[0] = '\0';

        if (item->title != NULL) {
            AmLatin1Copy(titleFold, sizeof(titleFold), (STRPTR)item->title);
        } else {
            strcpy((char *)titleFold, "(untitled)");
        }

        AmFormatShortDate(item->pubdate, shortDate, sizeof(shortDate));

        if (item->author != NULL && item->author[0] != '\0') {
            AmLatin1Copy(srcFold, sizeof(srcFold), (STRPTR)item->author);
        } else if (sourceHint != NULL) {
            AmLatin1Copy(srcFold, sizeof(srcFold), sourceHint);
        }

        if (shortDate[0] != '\0' && srcFold[0] != '\0') {
            sprintf((char *)meta, "%s - %s", (char *)shortDate,
                (char *)srcFold);
        } else if (shortDate[0] != '\0') {
            strcpy((char *)meta, (char *)shortDate);
        } else if (srcFold[0] != '\0') {
            strcpy((char *)meta, (char *)srcFold);
        }

        if (strlen((char *)meta) > 28) {
            meta[28] = '\0';
        }

        node = AllocListBrowserNode(2,
            LBNA_UserData, (ULONG)item,
            LBNA_Column, 0,
                LBNCA_CopyText, TRUE,
                LBNCA_Text, (ULONG)titleFold,
            LBNA_Column, 1,
                LBNCA_CopyText, TRUE,
                LBNCA_Text, (ULONG)meta,
            TAG_DONE);
        if (node != NULL) {
            AddTail(&gui->ag_ArticleNodes, node);
            if (AmLogIsOn()) {
                AmLogStringCheck((STRPTR)"lb.article.title",
                    (STRPTR)titleFold);
                AmLogStringCheck((STRPTR)"lb.article.meta", (STRPTR)meta);
            }
        }
        item = item->next;
    }

    arts_attach(gui);
}

void
AmigamiArticlesShowChannel(struct AmigamiGui *gui, struct FeedChannel *ch,
    STRPTR sourceHint)
{
    AmigamiArticlesClear(gui);
    AmigamiPreviewShowHint(gui, (STRPTR)"Select an article to read");
    add_item_nodes(gui, ch, sourceHint);
}

void
AmigamiArticlesShowFeed(struct AmigamiGui *gui, struct AmFeed *feed)
{
    if (feed == NULL || feed->af_Channel == NULL) {
        AmigamiArticlesClear(gui);
        AmigamiPreviewShowHint(gui, (STRPTR)"Select an article to read");
        return;
    }
    AmigamiArticlesShowChannel(gui, feed->af_Channel, feed->af_Title);
}

static void
mark_item_read(struct AmigamiGui *gui, struct FeedItem *item)
{
    struct AmFeed *feed;
    struct FeedItem *it;

    if (item == NULL) {
        return;
    }
    item->fi_Read = TRUE;

    /* Smart Today/All use shallow copies - mirror read onto originals. */
    if (gui == NULL || item->link == NULL || item->link[0] == '\0') {
        return;
    }
    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        if (feed->af_Channel != NULL) {
            it = feed->af_Channel->items;
            while (it != NULL) {
                if (it != item && it->link != NULL &&
                    stricmp((char *)it->link, (char *)item->link) == 0) {
                    it->fi_Read = TRUE;
                }
                it = it->next;
            }
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }
}

void
AmigamiArticlesHandleSelect(struct AmigamiGui *gui)
{
    struct Node *node;
    ULONG userdata;
    struct FeedItem *item;

    node = NULL;
    GetAttr(LISTBROWSER_SelectedNode, gui->ag_ArticlesLB, (ULONG *)&node);
    if (node == NULL) {
        gui->ag_SelectedItem = NULL;
        AmigamiPreviewShowHint(gui, (STRPTR)"Select an article to read");
        return;
    }

    userdata = 0;
    GetListBrowserNodeAttrs(node, LBNA_UserData, &userdata, TAG_DONE);
    item = (struct FeedItem *)userdata;
    gui->ag_SelectedItem = item;
    if (item != NULL) {
        mark_item_read(gui, item);
        AmigamiPreviewShowItem(gui, item);
        AmigamiGuiRefreshScreenTitle(gui);
    }
}
