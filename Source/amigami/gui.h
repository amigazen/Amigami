/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui.h - Amigami ReAction GUI shared state (NetNewsWire three-pane)
 */

#ifndef AMIGAMI_GUI_H
#define AMIGAMI_GUI_H

#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>

#include <proto/window.h>
#include <classes/window.h>
#include <proto/layout.h>
#include <gadgets/layout.h>
#include <proto/button.h>
#include <gadgets/button.h>
#include <proto/string.h>
#include <gadgets/string.h>
#include <proto/label.h>
#include <images/label.h>
#include <proto/listbrowser.h>
#include <gadgets/listbrowser.h>
#include <proto/scroller.h>
#include <gadgets/scroller.h>
#include <proto/speedbar.h>
#include <gadgets/speedbar.h>
#include <gadgets/richtextbrowser.h>
#include <images/bevel.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>
#include <proto/alib.h>

#include "feedstore.h"
#include "fetch.h"
#include "rss.h"

/* Optional tb.lib - present when linked; TBInit may fail gracefully. */
#include <libraries/tb.h>

/* Gadget IDs */
enum {
    GID_BTN_ADD = 1,
    GID_BTN_REMOVE,
    GID_BTN_REFRESH,
    GID_BTN_OPEN,
    GID_STATUS,
    GID_FEEDS,
    GID_ARTICLES,
    GID_PREVIEW,
    GID_SCROLLER,
    GID_SPEEDBAR,
    GID_ADD_URL,
    GID_ADD_OK,
    GID_ADD_CANCEL
};

/* Menu userdata IDs (WINDOW_NewMenu nm_UserData) */
enum {
    MID_QUIT = 1,
    MID_ADD_FEED,
    MID_REMOVE_FEED,
    MID_REFRESH,
    MID_OPEN,
    MID_TOGGLE_FEEDS
};

/* Speedbar button IDs (ln_Pri / code) */
enum {
    SBA_ADD = 1,
    SBA_REMOVE,
    SBA_REFRESH,
    SBA_OPEN
};

/* Left-pane selection kind (NetNewsWire-style smart feeds). */
enum {
    AMVIEW_NONE = 0,
    AMVIEW_TODAY,
    AMVIEW_ALL,
    AMVIEW_FEED
};

/* Listbrowser UserData sentinels (never collide with AmFeed *). */
#define AMSEL_TODAY  ((APTR)1)
#define AMSEL_ALL    ((APTR)2)

struct AmigamiGui
{
    Object             *ag_WinObj;
    Object             *ag_Layout;
    Object             *ag_MainRow;      /* HLayout: feeds | articles | preview */
    Object             *ag_FeedsLB;
    Object             *ag_ArticlesLB;
    Object             *ag_Rtb;
    Object             *ag_Scroller;
    Object             *ag_Status;
    Object             *ag_BtnAdd;       /* fallback text buttons */
    Object             *ag_BtnRemove;
    Object             *ag_BtnRefresh;
    Object             *ag_BtnOpen;
    Object             *ag_SpeedBar;     /* tb/speedbar when available */
    struct Window      *ag_Window;

    struct List         ag_FeedNodes;
    struct List         ag_ArticleNodes;
    struct List         ag_SbButtons;
    struct ColumnInfo  *ag_FeedCI;
    struct ColumnInfo  *ag_ArticleCI;

    Object             *ag_SbLabels[4];
    TBImage            *ag_SbIcons[4];
    ULONG               ag_SbCount;
    BOOL                ag_TBInited;
    BOOL                ag_UseSpeedBar;
    BOOL                ag_FeedsVisible;

    struct AmFeedStore  ag_Store;
    struct AmFetchSession ag_Fetch;
    struct AmFeed      *ag_SelectedFeed;
    struct FeedItem    *ag_SelectedItem;
    ULONG               ag_ViewMode;       /* AMVIEW_* */
    struct FeedChannel *ag_SmartChannel; /* owned aggregate for Today/All */

    WORD                ag_PenFg;
    WORD                ag_PenFill;
    UBYTE               ag_StatusBuf[160];
    UBYTE               ag_CaFile[256];
    BOOL                ag_Verbose;
    BOOL                ag_Insecure;
    BOOL                ag_QuitRequested;
};

#define AMIGAMI_DEFAULT_CA "DEVS:Certificates/cacert.pem"

LONG AmigamiGuiRun(STRPTR cafile, BOOL insecure, BOOL verbose);

void AmigamiGuiSetStatus(struct AmigamiGui *gui, STRPTR text);
void AmigamiGuiToggleFeeds(struct AmigamiGui *gui);

BOOL AmigamiFeedsInit(struct AmigamiGui *gui);
void AmigamiFeedsFree(struct AmigamiGui *gui);
void AmigamiFeedsRebuild(struct AmigamiGui *gui);
void AmigamiFeedsHandleSelect(struct AmigamiGui *gui);
BOOL AmigamiFeedsAddDialog(struct AmigamiGui *gui);
void AmigamiFeedsRemoveSelected(struct AmigamiGui *gui);

BOOL AmigamiArticlesInit(struct AmigamiGui *gui);
void AmigamiArticlesFree(struct AmigamiGui *gui);
void AmigamiArticlesClear(struct AmigamiGui *gui);
void AmigamiArticlesShowFeed(struct AmigamiGui *gui, struct AmFeed *feed);
void AmigamiArticlesShowChannel(struct AmigamiGui *gui,
    struct FeedChannel *ch, STRPTR sourceHint);
void AmigamiArticlesHandleSelect(struct AmigamiGui *gui);
BOOL AmItemIsToday(STRPTR pubdate);

BOOL AmigamiEnsureFeedLoaded(struct AmigamiGui *gui, struct AmFeed *feed,
    BOOL force);
void AmigamiShowSmartToday(struct AmigamiGui *gui, BOOL forceFetch);
void AmigamiShowSmartAll(struct AmigamiGui *gui, BOOL forceFetch);

void AmigamiRefreshSelected(struct AmigamiGui *gui);
void AmigamiOpenSelectedLink(struct AmigamiGui *gui);

BOOL AmigamiSpeedBarInit(struct AmigamiGui *gui);
void AmigamiSpeedBarDetach(struct AmigamiGui *gui);
void AmigamiSpeedBarFree(struct AmigamiGui *gui);
void AmigamiSpeedBarHandle(struct AmigamiGui *gui, UWORD code);

#endif /* AMIGAMI_GUI_H */
