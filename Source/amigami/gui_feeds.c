/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_feeds.c - Feed listbrowser + Today / All Feeds + add/remove
 *
 * listbrowser.gadget does not support true multiline rows; article pane
 * uses a compact Title + Date layout instead.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"
#include "feedcache.h"
#include "utf8fold.h"
#include "rtb_article.h"
#include "amlog.h"

static void
feeds_detach(struct AmigamiGui *gui)
{
    if (gui->ag_FeedsLB != NULL && gui->ag_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->ag_FeedsLB, gui->ag_Window, NULL,
            LISTBROWSER_Labels, ~0,
            TAG_DONE);
    } else if (gui->ag_FeedsLB != NULL) {
        SetAttrs(gui->ag_FeedsLB, LISTBROWSER_Labels, ~0, TAG_DONE);
    }
}

static void
feeds_attach(struct AmigamiGui *gui)
{
    if (gui->ag_FeedsLB != NULL && gui->ag_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->ag_FeedsLB, gui->ag_Window, NULL,
            LISTBROWSER_Labels, (ULONG)&gui->ag_FeedNodes,
            TAG_DONE);
    } else if (gui->ag_FeedsLB != NULL) {
        SetAttrs(gui->ag_FeedsLB,
            LISTBROWSER_Labels, (ULONG)&gui->ag_FeedNodes,
            TAG_DONE);
    }
}

static void
feeds_free_nodes(struct AmigamiGui *gui)
{
    feeds_detach(gui);
    FreeListBrowserList(&gui->ag_FeedNodes);
    NewList(&gui->ag_FeedNodes);
}

static struct Node *
add_feed_node(struct AmigamiGui *gui, APTR userdata, STRPTR text,
    STRPTR countText, BOOL readonly)
{
    struct Node *node;
    ULONG flags;
    STRPTR count;

    flags = 0;
    if (readonly) {
        flags = LBFLG_READONLY;
    }
    count = countText;
    if (count == NULL) {
        count = (STRPTR)"";
    }
    node = AllocListBrowserNode(2,
        LBNA_UserData, (ULONG)userdata,
        LBNA_Flags, flags,
        LBNA_Column, 0,
            LBNCA_CopyText, TRUE,
            LBNCA_Text, (ULONG)text,
        LBNA_Column, 1,
            LBNCA_CopyText, TRUE,
            LBNCA_Text, (ULONG)count,
        TAG_DONE);
    if (node != NULL) {
        AddTail(&gui->ag_FeedNodes, node);
        AmLogStringCheck((STRPTR)"lb.feed", text);
    }
    return node;
}

/* Update the "#" column for one subscription without rebuilding the list. */
static void
feeds_update_count(struct AmigamiGui *gui, struct AmFeed *feed)
{
    struct Node *node;
    APTR userdata;
    UBYTE countBuf[16];
    LONG nItems;

    if (gui == NULL || feed == NULL || gui->ag_FeedsLB == NULL) {
        return;
    }

    nItems = 0;
    countBuf[0] = '\0';
    if (feed->af_Channel != NULL) {
        nItems = feed->af_Channel->itemcount;
        if (nItems > 0) {
            sprintf((char *)countBuf, "%ld", (long)nItems);
        }
    }

    node = gui->ag_FeedNodes.lh_Head;
    while (node != NULL && node->ln_Succ != NULL) {
        userdata = NULL;
        GetListBrowserNodeAttrs(node, LBNA_UserData, (ULONG *)&userdata,
            TAG_DONE);
        if (userdata == (APTR)feed) {
            /* Node text update while list stays attached. */
            SetListBrowserNodeAttrs(node,
                LBNA_Column, 1,
                    LBNCA_CopyText, TRUE,
                    LBNCA_Text, (ULONG)countBuf,
                TAG_DONE);
            break;
        }
        node = node->ln_Succ;
    }
}

BOOL
AmigamiFeedsInit(struct AmigamiGui *gui)
{
    NewList(&gui->ag_FeedNodes);
    gui->ag_FeedCI = AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, (ULONG)"Feeds",
            LBCIA_Weight, 85,
            LBCIA_Flags, CIF_WEIGHTED,
        LBCIA_Column, 1,
            LBCIA_Title, (ULONG)"#",
            LBCIA_Weight, 15,
            LBCIA_Flags, CIF_WEIGHTED,
        TAG_DONE);
    if (gui->ag_FeedCI != NULL) {
        return TRUE;
    }
    return FALSE;
}

void
AmigamiFeedsFree(struct AmigamiGui *gui)
{
    feeds_free_nodes(gui);
    if (gui->ag_FeedCI != NULL) {
        FreeLBColumnInfo(gui->ag_FeedCI);
        gui->ag_FeedCI = NULL;
    }
    if (gui->ag_SmartChannel != NULL) {
        FreeFeedChannel(gui->ag_SmartChannel);
        gui->ag_SmartChannel = NULL;
    }
}

void
AmigamiFeedsRebuild(struct AmigamiGui *gui)
{
    struct AmFeed *feed;
    struct AmFeed *keepFeed;
    ULONG keepView;
    struct Node *node;
    struct Node *selNode;
    UBYTE disp[AMFEED_TITLE_MAX];
    UBYTE countBuf[16];
    LONG selIndex;
    LONG nItems;

    keepFeed = gui->ag_SelectedFeed;
    keepView = gui->ag_ViewMode;
    selNode = NULL;
    selIndex = -1;

    feeds_free_nodes(gui);

    node = add_feed_node(gui, AMSEL_TODAY, (STRPTR)"Today", (STRPTR)"",
        FALSE);
    if (keepView == AMVIEW_TODAY) {
        selNode = node;
    }
    node = add_feed_node(gui, AMSEL_ALL, (STRPTR)"All Feeds", (STRPTR)"",
        FALSE);
    if (keepView == AMVIEW_ALL) {
        selNode = node;
    }

    /* Blank spacer row (readonly) between smart feeds and subscriptions. */
    add_feed_node(gui, NULL, (STRPTR)" ", (STRPTR)"", TRUE);

    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        AmLatin1Copy(disp, sizeof(disp), (STRPTR)feed->af_Title);

        nItems = 0;
        countBuf[0] = '\0';
        if (feed->af_Channel != NULL) {
            nItems = feed->af_Channel->itemcount;
            if (nItems > 0) {
                sprintf((char *)countBuf, "%ld", (long)nItems);
            }
        }

        node = add_feed_node(gui, (APTR)feed, (STRPTR)disp,
            (STRPTR)countBuf, FALSE);
        if (keepView == AMVIEW_FEED && feed == keepFeed) {
            selNode = node;
        }
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }

    feeds_attach(gui);

    if (selNode != NULL && gui->ag_FeedsLB != NULL) {
        selIndex = 0;
        node = gui->ag_FeedNodes.lh_Head;
        while (node != NULL && node->ln_Succ != NULL) {
            if (node == selNode) {
                break;
            }
            selIndex++;
            node = node->ln_Succ;
        }
        if (gui->ag_Window != NULL) {
            SetGadgetAttrs((struct Gadget *)gui->ag_FeedsLB, gui->ag_Window,
                NULL,
                LISTBROWSER_Selected, selIndex,
                LISTBROWSER_MakeVisible, selIndex,
                TAG_DONE);
        } else {
            SetAttrs(gui->ag_FeedsLB,
                LISTBROWSER_Selected, selIndex,
                TAG_DONE);
        }
    }
}

void
AmigamiFeedsHandleSelect(struct AmigamiGui *gui)
{
    struct Node *node;
    APTR userdata;
    struct AmFeed *feed;

    node = NULL;
    GetAttr(LISTBROWSER_SelectedNode, gui->ag_FeedsLB, (ULONG *)&node);
    if (node == NULL) {
        gui->ag_SelectedFeed = NULL;
        gui->ag_ViewMode = AMVIEW_NONE;
        AmigamiArticlesClear(gui);
        AmigamiPreviewShowHint(gui, (STRPTR)"Select Today, All Feeds, or a feed");
        return;
    }

    userdata = NULL;
    GetListBrowserNodeAttrs(node, LBNA_UserData, (ULONG *)&userdata, TAG_DONE);

    if (userdata == NULL) {
        /* Separator - ignore */
        return;
    }

    if (userdata == AMSEL_TODAY) {
        AmigamiShowSmartToday(gui, FALSE);
        return;
    }
    if (userdata == AMSEL_ALL) {
        AmigamiShowSmartAll(gui, FALSE);
        return;
    }

    feed = (struct AmFeed *)userdata;
    gui->ag_ViewMode = AMVIEW_FEED;
    gui->ag_SelectedFeed = feed;
    gui->ag_SelectedItem = NULL;

    if (gui->ag_SmartChannel != NULL) {
        FreeFeedChannel(gui->ag_SmartChannel);
        gui->ag_SmartChannel = NULL;
    }

    if (!AmigamiEnsureFeedLoaded(gui, feed, FALSE)) {
        AmigamiArticlesClear(gui);
        AmigamiPreviewShowHint(gui,
            (STRPTR)"Could not load feed (try Refresh)");
        return;
    }

    /* Update item count in-place - full rebuild flickers and jumps. */
    feeds_update_count(gui, feed);
    AmigamiArticlesShowFeed(gui, feed);
    AmigamiGuiRefreshScreenTitle(gui);
}

BOOL
AmigamiFeedsAddDialog(struct AmigamiGui *gui)
{
    Object *winobj;
    Object *layout;
    Object *urlGad;
    Object *okGad;
    Object *cancelGad;
    struct Window *win;
    BOOL ok;
    BOOL done;
    UBYTE urlBuf[AMFEED_URL_MAX];

    ok = FALSE;
    urlBuf[0] = '\0';

    urlGad = StringObject,
        GA_ID, GID_ADD_URL,
        GA_RelVerify, TRUE,
        GA_TabCycle, TRUE,
        STRINGA_MaxChars, AMFEED_URL_MAX - 1,
        STRINGA_MinVisible, 40,
        STRINGA_TextVal, "https://",
    StringEnd;

    okGad = ButtonObject,
        GA_ID, GID_ADD_OK,
        GA_RelVerify, TRUE,
        GA_Text, "_OK",
    ButtonEnd;

    cancelGad = ButtonObject,
        GA_ID, GID_ADD_CANCEL,
        GA_RelVerify, TRUE,
        GA_Text, "_Cancel",
    ButtonEnd;

    if (urlGad == NULL || okGad == NULL || cancelGad == NULL) {
        return FALSE;
    }

    layout = VLayoutObject,
        LAYOUT_SpaceOuter, TRUE,
        LAYOUT_AddChild, urlGad,
        CHILD_Label, LabelObject,
            LABEL_Text, "Feed URL",
        LabelEnd,
        LAYOUT_AddChild, HLayoutObject,
            LAYOUT_AddChild, okGad,
            LAYOUT_AddChild, cancelGad,
        LayoutEnd,
        CHILD_WeightedHeight, 0,
    LayoutEnd;

    if (layout == NULL) {
        return FALSE;
    }

    winobj = WindowObject,
        WA_Title, "Add subscription",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_InnerWidth, 360,
        WA_InnerHeight, 80,
        WINDOW_Position, WPOS_CENTERMOUSE,
        WINDOW_ParentGroup, layout,
    EndWindow;

    if (winobj == NULL) {
        DisposeObject(layout);
        return FALSE;
    }

    win = (struct Window *)RA_OpenWindow(winobj);
    if (win == NULL) {
        DisposeObject(winobj);
        return FALSE;
    }

    done = FALSE;
    while (!done) {
        ULONG signal;
        ULONG got;
        ULONG result;
        UWORD code;

        GetAttr(WINDOW_SigMask, winobj, &signal);
        got = Wait(signal | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) {
            done = TRUE;
            break;
        }
        code = 0;
        while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW:
                done = TRUE;
                break;
            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case GID_ADD_OK:
                case GID_ADD_URL:
                    {
                        STRPTR val;

                        val = NULL;
                        GetAttr(STRINGA_TextVal, urlGad, (ULONG *)&val);
                        if (val != NULL && val[0] != '\0') {
                            strncpy((char *)urlBuf, (char *)val,
                                sizeof(urlBuf) - 1);
                            urlBuf[sizeof(urlBuf) - 1] = '\0';
                            ok = TRUE;
                        }
                        done = TRUE;
                    }
                    break;
                case GID_ADD_CANCEL:
                    done = TRUE;
                    break;
                }
                break;
            }
        }
    }

    RA_CloseWindow(winobj);
    DisposeObject(winobj);

    if (ok) {
        struct AmFeed *feed;

        feed = AmFeedStoreAdd(&gui->ag_Store, urlBuf, NULL);
        if (feed == NULL) {
            AmigamiGuiSetStatus(gui, (STRPTR)"Already subscribed or add failed");
            return FALSE;
        }
        AmFeedStoreSave(&gui->ag_Store);
        AmigamiFeedsRebuild(gui);
        AmigamiGuiRefreshScreenTitle(gui);
        return TRUE;
    }
    return FALSE;
}

void
AmigamiFeedsRemoveSelected(struct AmigamiGui *gui)
{
    if (gui->ag_ViewMode != AMVIEW_FEED || gui->ag_SelectedFeed == NULL) {
        AmigamiGuiSetStatus(gui, (STRPTR)"Select a subscription to remove");
        return;
    }
    AmFeedCacheDelete(gui->ag_SelectedFeed->af_Url);
    AmFeedStoreRemove(&gui->ag_Store, gui->ag_SelectedFeed);
    gui->ag_SelectedFeed = NULL;
    gui->ag_SelectedItem = NULL;
    gui->ag_ViewMode = AMVIEW_NONE;
    AmFeedStoreSave(&gui->ag_Store);
    AmigamiFeedsRebuild(gui);
    AmigamiArticlesClear(gui);
    AmigamiPreviewShowHint(gui, (STRPTR)"Select Today, All Feeds, or a feed");
    AmigamiGuiRefreshScreenTitle(gui);
}
