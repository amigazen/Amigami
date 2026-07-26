/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_speedbar.c - Load/cache/refresh + OpenURL
 *
 * Select path: memory → T:Amigami cache → fetch.
 * Refresh button: force network reload of the current feed (or all for smart).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/datetime.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"
#include "feedcache.h"
#include "rtb_article.h"
#include "amlog.h"

#include <proto/speedbar.h>
#include <proto/label.h>
#include <graphics/text.h>
#include <libraries/tb.h>
#include <libraries/tb_symbols.h>

extern struct Library *SpeedBarBase;

static Object *
sb_make_label(STRPTR text)
{
    return LabelObject,
        LABEL_Text, text,
        LABEL_Justification, LJ_CENTER,
        LABEL_SoftStyle, FSF_BOLD,
    LabelEnd;
}

static BOOL
sb_add_button(struct AmigamiGui *gui, ULONG id, STRPTR text,
    STRPTR icon_name, STRPTR help, BOOL use_icons)
{
    Object *lab;
    TBImage *img;
    struct Node *node;
    struct TagItem tags[5];
    ULONG ti;
    ULONG slot;

    slot = gui->ag_SbCount;
    if (slot >= 4) {
        return FALSE;
    }
    gui->ag_SbLabels[slot] = NULL;
    gui->ag_SbIcons[slot] = NULL;

    if (use_icons) {
        img = NewTBImage(icon_name, NULL, PRECISION_ICON, NULL);
        if (img != NULL) {
            ti = 0;
            tags[ti].ti_Tag = SBNA_Spacing;
            tags[ti].ti_Data = 2;
            ti++;
            tags[ti].ti_Tag = SBNA_Highlight;
            tags[ti].ti_Data = SBH_RECESS;
            ti++;
            if (help != NULL) {
                tags[ti].ti_Tag = SBNA_Help;
                tags[ti].ti_Data = (ULONG)help;
                ti++;
            }
            tags[ti].ti_Tag = TAG_DONE;
            tags[ti].ti_Data = 0;
            node = NewSpeedButtonNodeFromTBImage(img, TBA_Size24, id,
                NULL, NULL, tags);
            if (node != NULL) {
                node->ln_Pri = (BYTE)id;
                gui->ag_SbIcons[slot] = img;
                AddTail(&gui->ag_SbButtons, node);
                gui->ag_SbCount++;
                return TRUE;
            }
            DisposeTBImage(img);
        }
    }

    lab = sb_make_label(text);
    if (lab == NULL) {
        return FALSE;
    }
    if (help != NULL) {
        node = AllocSpeedButtonNode(id,
            SBNA_Image, (ULONG)lab,
            SBNA_Enabled, TRUE,
            SBNA_Spacing, 2,
            SBNA_Highlight, SBH_RECESS,
            SBNA_Help, (ULONG)help,
            TAG_DONE);
    } else {
        node = AllocSpeedButtonNode(id,
            SBNA_Image, (ULONG)lab,
            SBNA_Enabled, TRUE,
            SBNA_Spacing, 2,
            SBNA_Highlight, SBH_RECESS,
            TAG_DONE);
    }
    if (node == NULL) {
        DisposeObject(lab);
        return FALSE;
    }
    gui->ag_SbLabels[slot] = lab;
    AddTail(&gui->ag_SbButtons, node);
    gui->ag_SbCount++;
    return TRUE;
}

BOOL
AmigamiSpeedBarInit(struct AmigamiGui *gui)
{
    BOOL use_icons;

    if (gui == NULL) {
        return FALSE;
    }
    NewList(&gui->ag_SbButtons);
    gui->ag_SbCount = 0;
    gui->ag_TBInited = FALSE;
    gui->ag_UseSpeedBar = FALSE;

    /* tb.lib opens speedbar.gadget via TBInit when present. */
    TBInit();
    gui->ag_TBInited = TRUE;

    if (SpeedBarBase == NULL) {
        AmLog("Amigami: speedbar.gadget missing - text buttons\n");
        return TRUE; /* fallback buttons created in gui_main */
    }

    use_icons = (TBImagesExists() == TBERR_NOERROR) ? TRUE : FALSE;
    if (use_icons) {
        AmLog("Amigami: TBIMAGES: found - AISS icons\n");
    } else {
        AmLog("Amigami: TBIMAGES: missing - text speedbar faces\n");
    }

    if (!sb_add_button(gui, SBA_ADD, (STRPTR)"Add",
            (STRPTR)TB_SYM_NEW,
            (STRPTR)"Add a feed subscription", use_icons)) {
        return FALSE;
    }
    if (!sb_add_button(gui, SBA_REMOVE, (STRPTR)"Remove",
            (STRPTR)TB_SYM_DELETE,
            (STRPTR)"Remove the selected feed", use_icons)) {
        return FALSE;
    }
    if (!sb_add_button(gui, SBA_REFRESH, (STRPTR)"Refresh",
            (STRPTR)TB_SYM_RELOAD,
            (STRPTR)"Force-reload current feed from the network",
            use_icons)) {
        return FALSE;
    }
    if (!sb_add_button(gui, SBA_OPEN, (STRPTR)"Open",
            (STRPTR)TB_SYM_OPEN,
            (STRPTR)"Open the selected article in a browser",
            use_icons)) {
        return FALSE;
    }

    gui->ag_UseSpeedBar = TRUE;
    return TRUE;
}

void
AmigamiSpeedBarDetach(struct AmigamiGui *gui)
{
    if (gui == NULL || gui->ag_SpeedBar == NULL) {
        return;
    }
    if (gui->ag_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->ag_SpeedBar, gui->ag_Window,
            NULL, SPEEDBAR_Buttons, ~0, TAG_DONE);
    } else {
        SetAttrs(gui->ag_SpeedBar, SPEEDBAR_Buttons, ~0, TAG_DONE);
    }
}

void
AmigamiSpeedBarFree(struct AmigamiGui *gui)
{
    struct Node *n;
    ULONG i;

    if (gui == NULL) {
        return;
    }
    while ((n = RemHead(&gui->ag_SbButtons)) != NULL) {
        FreeSpeedButtonNode(n);
    }
    NewList(&gui->ag_SbButtons);

    for (i = 0; i < gui->ag_SbCount; i++) {
        if (gui->ag_SbLabels[i] != NULL) {
            DisposeObject(gui->ag_SbLabels[i]);
            gui->ag_SbLabels[i] = NULL;
        }
        if (gui->ag_SbIcons[i] != NULL) {
            DisposeTBImage(gui->ag_SbIcons[i]);
            gui->ag_SbIcons[i] = NULL;
        }
    }
    gui->ag_SbCount = 0;

    if (gui->ag_TBInited) {
        TBExit();
        gui->ag_TBInited = FALSE;
    }
}

void
AmigamiSpeedBarHandle(struct AmigamiGui *gui, UWORD code)
{
    if (gui == NULL) {
        return;
    }
    switch (code) {
    case SBA_ADD:
        AmigamiFeedsAddDialog(gui);
        break;
    case SBA_REMOVE:
        AmigamiFeedsRemoveSelected(gui);
        break;
    case SBA_REFRESH:
        AmigamiRefreshSelected(gui);
        break;
    case SBA_OPEN:
        AmigamiOpenSelectedLink(gui);
        break;
    }
}

static void
busy(struct AmigamiGui *gui, BOOL on)
{
    if (gui != NULL && gui->ag_WinObj != NULL) {
        SetAttrs(gui->ag_WinObj, WA_BusyPointer, on ? TRUE : FALSE, TAG_DONE);
    }
}

static BOOL
parse_into_feed(struct AmFeed *feed, UBYTE *body, ULONG len, STRPTR errBuf,
    ULONG errMax)
{
    struct FeedChannel *ch;

    ch = ParseRssBuffer(body, (long)len);
    if (ch == NULL) {
        if (errBuf != NULL && errMax > 0) {
            strcpy((char *)errBuf, "Parse failed");
        }
        return FALSE;
    }
    AmFeedSetChannel(feed, ch);
    feed->af_LastError[0] = '\0';
    return TRUE;
}

BOOL
AmigamiEnsureFeedLoaded(struct AmigamiGui *gui, struct AmFeed *feed,
    BOOL force)
{
    UBYTE *body;
    ULONG len;
    LONG status;
    UBYTE err[AMFEED_ERR_MAX];

    if (gui == NULL || feed == NULL) {
        return FALSE;
    }

    if (!force && feed->af_Channel != NULL) {
        if (feed->af_Channel->itemcount > 0) {
            AmLog("Amigami: load \"%s\" from memory (%ld items)\n",
                feed->af_Title, (long)feed->af_Channel->itemcount);
            return TRUE;
        }
        AmLog("Amigami: memory cache empty for \"%s\" - reload\n",
            feed->af_Title);
        AmFeedClearChannel(feed);
    }

    body = NULL;
    len = 0;
    err[0] = '\0';

    if (!force) {
        if (AmFeedCacheLoad(feed->af_Url, &body, &len)) {
            AmLog("Amigami: load \"%s\" from T: cache (%lu bytes)\n",
                feed->af_Title, (unsigned long)len);
            if (parse_into_feed(feed, body, len, err, sizeof(err))) {
                FreeVec(body);
                return TRUE;
            }
            FreeVec(body);
            body = NULL;
            AmLog("Amigami: T: cache parse failed for %s - fetching\n",
                feed->af_Url);
        }
    }

    AmLog("Amigami: fetching %s (force=%ld)\n", feed->af_Url, (long)force);
    AmigamiGuiSetStatus(gui, (STRPTR)"Fetching...");
    busy(gui, TRUE);
    status = 0;
    {
        struct DateStamp t0;
        struct DateStamp t1;
        ULONG fetchTicks;
        ULONG parseTicks;

        DateStamp(&t0);
        if (!AmFetchUrl(&gui->ag_Fetch, feed->af_Url, &body, &len, &status,
            err, sizeof(err))) {
            busy(gui, FALSE);
            strncpy((char *)feed->af_LastError, (char *)err,
                sizeof(feed->af_LastError) - 1);
            AmigamiGuiSetStatus(gui,
                err[0] != '\0' ? err : (STRPTR)"Fetch failed");
            return FALSE;
        }
        DateStamp(&t1);
        fetchTicks = (ULONG)(t1.ds_Minute - t0.ds_Minute) * 50UL +
            (ULONG)(t1.ds_Tick - t0.ds_Tick);
        /* Day wrap / borrow when tick underflows across minutes */
        if (t1.ds_Tick < t0.ds_Tick) {
            fetchTicks = (ULONG)(t1.ds_Minute - t0.ds_Minute - 1) * 50UL +
                (ULONG)(t1.ds_Tick + 50 - t0.ds_Tick);
        }

        AmFeedCacheSave(feed->af_Url, body, len);

        DateStamp(&t0);
        if (!parse_into_feed(feed, body, len, err, sizeof(err))) {
            FreeVec(body);
            busy(gui, FALSE);
            strncpy((char *)feed->af_LastError, (char *)err,
                sizeof(feed->af_LastError) - 1);
            AmigamiGuiSetStatus(gui, (STRPTR)"Could not parse RSS/Atom");
            return FALSE;
        }
        DateStamp(&t1);
        parseTicks = (ULONG)(t1.ds_Minute - t0.ds_Minute) * 50UL +
            (ULONG)(t1.ds_Tick - t0.ds_Tick);
        if (t1.ds_Tick < t0.ds_Tick) {
            parseTicks = (ULONG)(t1.ds_Minute - t0.ds_Minute - 1) * 50UL +
                (ULONG)(t1.ds_Tick + 50 - t0.ds_Tick);
        }

        AmLog("Amigami: timing fetch=%lu ticks (~%lu ms) parse=%lu ticks "
            "(~%lu ms) bytes=%lu\n",
            (unsigned long)fetchTicks,
            (unsigned long)(fetchTicks * 20UL),
            (unsigned long)parseTicks,
            (unsigned long)(parseTicks * 20UL),
            (unsigned long)len);
    }
    FreeVec(body);
    busy(gui, FALSE);
    AmigamiGuiNoteSync(gui);
    AmigamiGuiRefreshScreenTitle(gui);
    return TRUE;
}

static void
smart_clear(struct AmigamiGui *gui)
{
    if (gui->ag_SmartChannel != NULL) {
        FreeFeedChannel(gui->ag_SmartChannel);
        gui->ag_SmartChannel = NULL;
    }
}

static struct FeedItem *
dup_item_shallow(struct FeedItem *src)
{
    struct FeedItem *it;
    ULONG n;

    if (src == NULL) {
        return NULL;
    }
    it = (struct FeedItem *)AllocVec(sizeof(struct FeedItem), MEMF_CLEAR);
    if (it == NULL) {
        return NULL;
    }
    if (src->title != NULL) {
        n = strlen((char *)src->title) + 1;
        it->title = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->title != NULL) {
            CopyMem(src->title, it->title, n);
            it->titlelen = src->titlelen;
        }
    }
    if (src->link != NULL) {
        n = strlen((char *)src->link) + 1;
        it->link = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->link != NULL) {
            CopyMem(src->link, it->link, n);
            it->linklen = src->linklen;
        }
    }
    if (src->description != NULL) {
        n = strlen((char *)src->description) + 1;
        it->description = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->description != NULL) {
            CopyMem(src->description, it->description, n);
            it->desclen = src->desclen;
        }
    }
    if (src->pubdate != NULL) {
        n = strlen((char *)src->pubdate) + 1;
        it->pubdate = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->pubdate != NULL) {
            CopyMem(src->pubdate, it->pubdate, n);
            it->pubdatelen = src->pubdatelen;
        }
    }
    if (src->author != NULL) {
        n = strlen((char *)src->author) + 1;
        it->author = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->author != NULL) {
            CopyMem(src->author, it->author, n);
            it->authorlen = src->authorlen;
        }
    } else if (src->guid != NULL) {
        /* reuse guid slot unused - leave author empty */
    }
    it->fi_Read = src->fi_Read;
    return it;
}

static void
smart_add_item(struct FeedChannel *ch, struct FeedItem *src,
    STRPTR feedTitle)
{
    struct FeedItem *it;

    it = dup_item_shallow(src);
    if (it == NULL) {
        return;
    }
    /* Prefer feed name as author when item has none (All/Today lists). */
    if (it->author == NULL && feedTitle != NULL) {
        ULONG n;

        n = strlen((char *)feedTitle) + 1;
        it->author = (UBYTE *)AllocVec(n, MEMF_ANY);
        if (it->author != NULL) {
            CopyMem(feedTitle, it->author, n);
            it->authorlen = (long)(n - 1);
        }
    }
    if (ch->lastitem != NULL) {
        ch->lastitem->next = it;
    } else {
        ch->items = it;
    }
    ch->lastitem = it;
    ch->itemcount++;
}

static BOOL
ensure_all_feeds(struct AmigamiGui *gui, BOOL forceFetch)
{
    struct AmFeed *feed;
    BOOL any;

    any = FALSE;
    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        if (AmigamiEnsureFeedLoaded(gui, feed, forceFetch)) {
            any = TRUE;
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }
    return any;
}

void
AmigamiShowSmartToday(struct AmigamiGui *gui, BOOL forceFetch)
{
    struct AmFeed *feed;
    struct FeedItem *item;
    struct FeedChannel *ch;

    gui->ag_ViewMode = AMVIEW_TODAY;
    gui->ag_SelectedFeed = NULL;
    smart_clear(gui);

    AmigamiGuiSetStatus(gui, (STRPTR)"Loading Today...");
    ensure_all_feeds(gui, forceFetch);

    ch = (struct FeedChannel *)AllocVec(sizeof(struct FeedChannel),
        MEMF_CLEAR);
    if (ch == NULL) {
        AmigamiGuiSetStatus(gui, (STRPTR)"Out of memory");
        return;
    }
    ch->title = (UBYTE *)AllocVec(8, MEMF_ANY);
    if (ch->title != NULL) {
        strcpy((char *)ch->title, "Today");
        ch->titlelen = 5;
    }

    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        if (feed->af_Channel != NULL) {
            item = feed->af_Channel->items;
            while (item != NULL) {
                if (AmItemIsToday(item->pubdate)) {
                    smart_add_item(ch, item, feed->af_Title);
                }
                item = item->next;
            }
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }

    gui->ag_SmartChannel = ch;
    AmigamiArticlesShowChannel(gui, ch, (STRPTR)"Today");
    AmigamiGuiRefreshScreenTitle(gui);
}

void
AmigamiShowSmartAll(struct AmigamiGui *gui, BOOL forceFetch)
{
    struct AmFeed *feed;
    struct FeedItem *item;
    struct FeedChannel *ch;

    gui->ag_ViewMode = AMVIEW_ALL;
    gui->ag_SelectedFeed = NULL;
    smart_clear(gui);

    AmigamiGuiSetStatus(gui, (STRPTR)"Loading All Feeds...");
    ensure_all_feeds(gui, forceFetch);

    ch = (struct FeedChannel *)AllocVec(sizeof(struct FeedChannel),
        MEMF_CLEAR);
    if (ch == NULL) {
        AmigamiGuiSetStatus(gui, (STRPTR)"Out of memory");
        return;
    }
    ch->title = (UBYTE *)AllocVec(12, MEMF_ANY);
    if (ch->title != NULL) {
        strcpy((char *)ch->title, "All Feeds");
        ch->titlelen = 9;
    }

    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        if (feed->af_Channel != NULL) {
            item = feed->af_Channel->items;
            while (item != NULL) {
                smart_add_item(ch, item, feed->af_Title);
                item = item->next;
            }
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }

    gui->ag_SmartChannel = ch;
    AmigamiArticlesShowChannel(gui, ch, (STRPTR)"All Feeds");
    AmigamiGuiRefreshScreenTitle(gui);
}

void
AmigamiRefreshSelected(struct AmigamiGui *gui)
{
    struct AmFeed *feed;

    if (gui->ag_ViewMode == AMVIEW_TODAY) {
        AmigamiShowSmartToday(gui, TRUE);
        return;
    }
    if (gui->ag_ViewMode == AMVIEW_ALL) {
        AmigamiShowSmartAll(gui, TRUE);
        return;
    }

    feed = gui->ag_SelectedFeed;
    if (feed == NULL) {
        AmigamiGuiSetStatus(gui, (STRPTR)"Select a feed to refresh");
        return;
    }

    if (!AmigamiEnsureFeedLoaded(gui, feed, TRUE)) {
        return;
    }

    AmigamiFeedsRebuild(gui);
    AmigamiArticlesShowFeed(gui, feed);
    AmigamiGuiRefreshScreenTitle(gui);
}

void
AmigamiOpenSelectedLink(struct AmigamiGui *gui)
{
    STRPTR url;
    UBYTE cmd[AMFEED_URL_MAX + 32];
    LONG rc;

    url = NULL;
    if (gui->ag_SelectedItem != NULL && gui->ag_SelectedItem->link != NULL) {
        url = (STRPTR)gui->ag_SelectedItem->link;
    } else if (gui->ag_SelectedFeed != NULL) {
        url = (STRPTR)gui->ag_SelectedFeed->af_Url;
    }

    if (url == NULL || url[0] == '\0') {
        AmigamiGuiSetStatus(gui, (STRPTR)"No link to open");
        return;
    }

    sprintf((char *)cmd, "OpenURL \"%s\"", (char *)url);
    rc = SystemTags(cmd,
        SYS_Input, NULL,
        SYS_Output, NULL,
        SYS_Asynch, TRUE,
        TAG_DONE);
    if (rc != 0) {
        AmigamiGuiSetStatus(gui, url);
    } else {
        AmigamiGuiSetStatus(gui, (STRPTR)"Opened via OpenURL");
    }
}
