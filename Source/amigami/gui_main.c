/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_main.c - Amigami main window (NetNewsWire three-pane layout)
 *
 *   +------------------------------------------------------------------+
 *   | [Add] [Remove] [Refresh] [Open]                                  |
 *   +----------+-------------------+-----------------------------------+
 *   | Feeds    || Articles         || Preview (largest) + scroller     |
 *   +----------+-------------------+-----------------------------------+
 *
 * Status is WA_ScreenTitle (persistent stats; ephemeral errors clear on
 * the next click/menu action).
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layout.h>
#include <dos/datetime.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"
#include "rtb_article.h"
#include "feedcache.h"
#include "amlog.h"

extern struct Library *WindowBase;
extern struct Library *LayoutBase;
extern struct Library *ButtonBase;
extern struct Library *StringBase;
extern struct Library *ListBrowserBase;
extern struct Library *ScrollerBase;
extern struct Library *LabelBase;
extern struct Library *RichTextBrowserBase;
extern struct Library *HttpBase;
extern struct Library *SpeedBarBase;

static struct NewMenu amigami_newmenu[] = {
    { NM_TITLE, (STRPTR)"Amigami", 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Add Feed...", (STRPTR)"A", 0, 0, (APTR)MID_ADD_FEED },
    { NM_ITEM,  (STRPTR)"Remove Feed", (STRPTR)"D", 0, 0, (APTR)MID_REMOVE_FEED },
    { NM_ITEM,  NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Refresh", (STRPTR)"R", 0, 0, (APTR)MID_REFRESH },
    { NM_ITEM,  (STRPTR)"Open Article", (STRPTR)"O", 0, 0, (APTR)MID_OPEN },
    { NM_ITEM,  NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Quit", (STRPTR)"Q", 0, 0, (APTR)MID_QUIT },

    { NM_TITLE, (STRPTR)"View", 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Show/Hide Feeds", (STRPTR)"F", 0, 0,
        (APTR)MID_TOGGLE_FEEDS },

    { NM_END, NULL, 0, 0, 0, 0 }
};

void
AmigamiGuiNoteSync(struct AmigamiGui *gui)
{
    if (gui == NULL) {
        return;
    }
    DateStamp(&gui->ag_LastSync);
    gui->ag_LastSyncValid = TRUE;
}

static ULONG
count_unread_in_channel(struct FeedChannel *ch)
{
    struct FeedItem *it;
    ULONG n;

    n = 0;
    if (ch == NULL) {
        return 0;
    }
    it = ch->items;
    while (it != NULL) {
        if (!it->fi_Read) {
            n++;
        }
        it = it->next;
    }
    return n;
}

static ULONG
count_unread_all(struct AmigamiGui *gui)
{
    struct AmFeed *feed;
    ULONG n;

    n = 0;
    if (gui == NULL) {
        return 0;
    }
    feed = (struct AmFeed *)gui->ag_Store.fs_Feeds.lh_Head;
    while (feed != NULL && feed->af_Node.ln_Succ != NULL) {
        n += count_unread_in_channel(feed->af_Channel);
        feed = (struct AmFeed *)feed->af_Node.ln_Succ;
    }
    return n;
}

static void
format_sync_ago(struct AmigamiGui *gui, UBYTE *out, ULONG outMax)
{
    struct DateStamp now;
    LONG mins;

    if (out == NULL || outMax == 0) {
        return;
    }
    out[0] = '\0';
    if (gui == NULL || !gui->ag_LastSyncValid) {
        strncpy((char *)out, "never synced", outMax - 1);
        out[outMax - 1] = '\0';
        return;
    }

    DateStamp(&now);
    mins = (LONG)(now.ds_Days - gui->ag_LastSync.ds_Days) * (24L * 60L) +
        (LONG)(now.ds_Minute - gui->ag_LastSync.ds_Minute);
    if (now.ds_Tick < gui->ag_LastSync.ds_Tick && mins > 0) {
        mins--;
    }
    if (mins < 0) {
        mins = 0;
    }

    if (mins < 1) {
        strncpy((char *)out, "just now", outMax - 1);
    } else if (mins == 1) {
        strncpy((char *)out, "1 min ago", outMax - 1);
    } else if (mins < 60) {
        sprintf((char *)out, "%ld min ago", (long)mins);
    } else if (mins < 120) {
        strncpy((char *)out, "1 hr ago", outMax - 1);
    } else if (mins < 24 * 60) {
        sprintf((char *)out, "%ld hr ago", (long)(mins / 60));
    } else if (mins < 48 * 60) {
        strncpy((char *)out, "1 day ago", outMax - 1);
    } else {
        sprintf((char *)out, "%ld days ago", (long)(mins / (24 * 60)));
    }
    out[outMax - 1] = '\0';
}

static void
apply_screen_title(struct AmigamiGui *gui)
{
    if (gui == NULL || gui->ag_Window == NULL) {
        return;
    }
    /* ~0 = leave window title unchanged */
    SetWindowTitles(gui->ag_Window, (STRPTR)~0, (STRPTR)gui->ag_ScreenTitle);
}

void
AmigamiGuiRefreshScreenTitle(struct AmigamiGui *gui)
{
    ULONG unread;
    ULONG feeds;
    UBYTE ago[40];

    if (gui == NULL) {
        return;
    }

    unread = count_unread_all(gui);
    feeds = gui->ag_Store.fs_Count;
    format_sync_ago(gui, ago, sizeof(ago));

    sprintf((char *)gui->ag_ScreenTitle,
        "Amigami - %lu unread, %lu feeds - %s",
        (unsigned long)unread, (unsigned long)feeds, (char *)ago);
    gui->ag_ScreenTitle[sizeof(gui->ag_ScreenTitle) - 1] = '\0';
    gui->ag_EphemeralTitle = FALSE;
    apply_screen_title(gui);
}

void
AmigamiGuiSetStatus(struct AmigamiGui *gui, STRPTR text)
{
    if (gui == NULL) {
        return;
    }
    if (text == NULL || text[0] == '\0') {
        AmigamiGuiRefreshScreenTitle(gui);
        return;
    }
    strncpy((char *)gui->ag_ScreenTitle, (char *)text,
        sizeof(gui->ag_ScreenTitle) - 1);
    gui->ag_ScreenTitle[sizeof(gui->ag_ScreenTitle) - 1] = '\0';
    gui->ag_EphemeralTitle = TRUE;
    apply_screen_title(gui);
}

void
AmigamiGuiNoteUserActivity(struct AmigamiGui *gui)
{
    if (gui == NULL) {
        return;
    }
    if (gui->ag_EphemeralTitle) {
        AmigamiGuiRefreshScreenTitle(gui);
    }
}

void
AmigamiGuiToggleFeeds(struct AmigamiGui *gui)
{
    if (gui == NULL || gui->ag_MainRow == NULL || gui->ag_FeedsLB == NULL) {
        return;
    }

    gui->ag_FeedsVisible = gui->ag_FeedsVisible ? FALSE : TRUE;

    if (gui->ag_FeedsVisible) {
        SetGadgetAttrs((struct Gadget *)gui->ag_FeedsLB, gui->ag_Window, NULL,
            GA_Hidden, FALSE, TAG_DONE);
        SetGadgetAttrs((struct Gadget *)gui->ag_MainRow, gui->ag_Window, NULL,
            LAYOUT_ModifyChild, (ULONG)gui->ag_FeedsLB,
                CHILD_WeightedWidth, 22,
                CHILD_MinWidth, 140,
                CHILD_WeightBar, TRUE,
            TAG_DONE);
        AmigamiGuiRefreshScreenTitle(gui);
    } else {
        SetGadgetAttrs((struct Gadget *)gui->ag_FeedsLB, gui->ag_Window, NULL,
            GA_Hidden, TRUE, TAG_DONE);
        SetGadgetAttrs((struct Gadget *)gui->ag_MainRow, gui->ag_Window, NULL,
            LAYOUT_ModifyChild, (ULONG)gui->ag_FeedsLB,
                CHILD_WeightedWidth, 0,
                CHILD_MinWidth, 0,
                CHILD_WeightBar, FALSE,
            TAG_DONE);
        AmigamiGuiRefreshScreenTitle(gui);
    }

    if (gui->ag_Window != NULL && gui->ag_Layout != NULL) {
        RethinkLayout((struct Gadget *)gui->ag_Layout, gui->ag_Window,
            NULL, TRUE);
    }
}

static void
gui_init_pens(struct AmigamiGui *gui)
{
    struct DrawInfo *dri;

    gui->ag_PenFg = 1;
    gui->ag_PenFill = 3;
    if (gui->ag_Window == NULL) {
        return;
    }
    dri = GetScreenDrawInfo(gui->ag_Window->WScreen);
    if (dri != NULL) {
        gui->ag_PenFg = dri->dri_Pens[TEXTPEN];
        gui->ag_PenFill = dri->dri_Pens[FILLPEN];
        FreeScreenDrawInfo(gui->ag_Window->WScreen, dri);
    }
}

static void
gui_build_hints(struct AmigamiGui *gui)
{
    ULONG n;

    n = 0;

    /* Speedbar buttons (one GA_ID, distinct hi_Code per button). */
    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_SPEEDBAR;
    gui->ag_Hints[n].hi_Code = (WORD)SBA_ADD;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Add a feed subscription";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_SPEEDBAR;
    gui->ag_Hints[n].hi_Code = (WORD)SBA_REMOVE;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Remove the selected feed";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_SPEEDBAR;
    gui->ag_Hints[n].hi_Code = (WORD)SBA_REFRESH;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Force-reload current feed from the network";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_SPEEDBAR;
    gui->ag_Hints[n].hi_Code = (WORD)SBA_OPEN;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Open the selected article in a browser";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    /* Fallback text toolbar (when speedbar.gadget / tb.lib unavailable). */
    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_BTN_ADD;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Add a feed subscription";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_BTN_REMOVE;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Remove the selected feed";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_BTN_REFRESH;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Force-reload current feed from the network";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_BTN_OPEN;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Open the selected article in a browser";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_FEEDS;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Select Today, All Feeds, or a subscription";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_ARTICLES;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Select an article to preview";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_PREVIEW;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Article preview - click links to open";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    gui->ag_Hints[n].hi_GadgetID = (WORD)GID_SCROLLER;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = (STRPTR)"Scroll the article preview";
    gui->ag_Hints[n].hi_Flags = 0;
    n++;

    /* Terminator */
    gui->ag_Hints[n].hi_GadgetID = -1;
    gui->ag_Hints[n].hi_Code = -1;
    gui->ag_Hints[n].hi_Text = NULL;
    gui->ag_Hints[n].hi_Flags = 0;
}

static BOOL
gui_open_main(struct AmigamiGui *gui)
{
    Object *top_row;
    Object *preview_row;
    Class *rtbClass;

    gui->ag_FeedsVisible = TRUE;

    if (!AmigamiFeedsInit(gui)) {
        PutStr("Amigami: feed column info failed\n");
        return FALSE;
    }
    if (!AmigamiArticlesInit(gui)) {
        PutStr("Amigami: article column info failed\n");
        return FALSE;
    }
    if (!AmigamiSpeedBarInit(gui)) {
        return FALSE;
    }

    strcpy((char *)gui->ag_ScreenTitle, "Amigami");

    if (RichTextBrowserBase == NULL) {
        PutStr("Amigami: richtextbrowser.gadget missing\n");
        return FALSE;
    }
    rtbClass = RICHTEXTBROWSER_GetClass();
    if (rtbClass == NULL) {
        PutStr("Amigami: RICHTEXTBROWSER_GetClass failed\n");
        return FALSE;
    }

    gui->ag_BtnAdd = NULL;
    gui->ag_BtnRemove = NULL;
    gui->ag_BtnRefresh = NULL;
    gui->ag_BtnOpen = NULL;
    gui->ag_SpeedBar = NULL;

    if (gui->ag_UseSpeedBar) {
        gui->ag_SpeedBar = SpeedBarObject,
            GA_ID, GID_SPEEDBAR,
            GA_RelVerify, TRUE,
            SPEEDBAR_Buttons, (ULONG)&gui->ag_SbButtons,
            SPEEDBAR_Orientation, SBORIENT_HORIZ,
            SPEEDBAR_BevelStyle, BVS_NONE,
        SpeedBarEnd;
        if (gui->ag_SpeedBar == NULL) {
            gui->ag_UseSpeedBar = FALSE;
        }
    }

    if (!gui->ag_UseSpeedBar) {
        gui->ag_BtnAdd = ButtonObject,
            GA_ID, GID_BTN_ADD,
            GA_RelVerify, TRUE,
            GA_Text, "_Add",
            GA_GadgetHelpText, (ULONG)"Add a feed subscription",
        ButtonEnd;
        gui->ag_BtnRemove = ButtonObject,
            GA_ID, GID_BTN_REMOVE,
            GA_RelVerify, TRUE,
            GA_Text, "Re_move",
            GA_GadgetHelpText, (ULONG)"Remove the selected feed",
        ButtonEnd;
        gui->ag_BtnRefresh = ButtonObject,
            GA_ID, GID_BTN_REFRESH,
            GA_RelVerify, TRUE,
            GA_Text, "_Refresh",
            GA_GadgetHelpText, (ULONG)"Force-reload current feed from the network",
        ButtonEnd;
        gui->ag_BtnOpen = ButtonObject,
            GA_ID, GID_BTN_OPEN,
            GA_RelVerify, TRUE,
            GA_Text, "_Open",
            GA_GadgetHelpText, (ULONG)"Open the selected article in a browser",
        ButtonEnd;
        if (gui->ag_BtnAdd == NULL || gui->ag_BtnRemove == NULL ||
            gui->ag_BtnRefresh == NULL || gui->ag_BtnOpen == NULL) {
            return FALSE;
        }
    }

    gui->ag_FeedsLB = ListBrowserObject,
        GA_ID, GID_FEEDS,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Select Today, All Feeds, or a subscription",
        LISTBROWSER_Labels, (ULONG)&gui->ag_FeedNodes,
        LISTBROWSER_ColumnInfo, (ULONG)gui->ag_FeedCI,
        LISTBROWSER_ColumnTitles, TRUE,
        LISTBROWSER_ShowSelected, TRUE,
        LISTBROWSER_Hierarchical, FALSE,
        LISTBROWSER_AutoFit, FALSE,
        LISTBROWSER_VerticalProp, TRUE,
        LISTBROWSER_MinVisible, 14,
    ListBrowserEnd;

    gui->ag_ArticlesLB = ListBrowserObject,
        GA_ID, GID_ARTICLES,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Select an article to preview",
        LISTBROWSER_Labels, (ULONG)&gui->ag_ArticleNodes,
        LISTBROWSER_ColumnInfo, (ULONG)gui->ag_ArticleCI,
        LISTBROWSER_ColumnTitles, TRUE,
        LISTBROWSER_ShowSelected, TRUE,
        LISTBROWSER_Striping, LBS_ROWS,
        LISTBROWSER_AutoFit, FALSE,
        LISTBROWSER_VerticalProp, TRUE,
        LISTBROWSER_HorizontalProp, TRUE,
        LISTBROWSER_MinVisible, 8,
    ListBrowserEnd;

    gui->ag_Rtb = NewObject(rtbClass, NULL,
        GA_ID, GID_PREVIEW,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Article preview - click links to open",
        RTB_SelectBlocks, FALSE,
        RTB_BlockCap, 64,
        RTB_Overscan, 32,
        TAG_DONE);

    gui->ag_Scroller = ScrollerObject,
        GA_ID, GID_SCROLLER,
        GA_RelVerify, TRUE,
        GA_Immediate, FALSE,
        GA_GadgetHelpText, (ULONG)"Scroll the article preview",
        SCROLLER_Orientation, SORIENT_VERT,
        SCROLLER_Arrows, TRUE,
    ScrollerEnd;

    if (gui->ag_FeedsLB == NULL || gui->ag_ArticlesLB == NULL ||
        gui->ag_Rtb == NULL || gui->ag_Scroller == NULL) {
        return FALSE;
    }

    if (gui->ag_UseSpeedBar) {
        top_row = HLayoutObject,
            LAYOUT_AddChild, gui->ag_SpeedBar,
            CHILD_WeightedWidth, 100,
        LayoutEnd;
    } else {
        top_row = HLayoutObject,
            LAYOUT_AddChild, gui->ag_BtnAdd,
            CHILD_WeightedWidth, 0,
            LAYOUT_AddChild, gui->ag_BtnRemove,
            CHILD_WeightedWidth, 0,
            LAYOUT_AddChild, gui->ag_BtnRefresh,
            CHILD_WeightedWidth, 0,
            LAYOUT_AddChild, gui->ag_BtnOpen,
            CHILD_WeightedWidth, 0,
        LayoutEnd;
    }

    preview_row = HLayoutObject,
        LAYOUT_AddChild, gui->ag_Rtb,
        CHILD_MinHeight, 120,
        CHILD_WeightedWidth, 100,
        LAYOUT_AddChild, gui->ag_Scroller,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 16,
    LayoutEnd;

    /* Preview is the reading pane - give it the largest weight. */
    gui->ag_MainRow = HLayoutObject,
        LAYOUT_SpaceOuter, FALSE,
        LAYOUT_AddChild, gui->ag_FeedsLB,
        CHILD_WeightedWidth, 22,
        CHILD_MinWidth, 140,
        CHILD_WeightedHeight, 100,
        CHILD_WeightBar, TRUE,
        LAYOUT_AddChild, gui->ag_ArticlesLB,
        CHILD_WeightedWidth, 28,
        CHILD_MinWidth, 160,
        CHILD_WeightedHeight, 100,
        CHILD_WeightBar, TRUE,
        LAYOUT_AddChild, preview_row,
        CHILD_WeightedWidth, 50,
        CHILD_MinWidth, 220,
        CHILD_WeightedHeight, 100,
    LayoutEnd;

    gui->ag_Layout = VLayoutObject,
        LAYOUT_SpaceOuter, TRUE,
        LAYOUT_DeferLayout, TRUE,
        LAYOUT_AddChild, top_row,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, gui->ag_MainRow,
        CHILD_WeightedHeight, 100,
        CHILD_MinHeight, 160,
    LayoutEnd;

    if (gui->ag_Layout == NULL || gui->ag_MainRow == NULL) {
        return FALSE;
    }

    gui_build_hints(gui);

    gui->ag_WinObj = WindowObject,
        WA_Title, "Amigami",
        WA_ScreenTitle, gui->ag_ScreenTitle,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_SmartRefresh, TRUE,
        WA_InnerWidth, 780,
        WA_InnerHeight, 440,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_HintInfo, (ULONG)gui->ag_Hints,
        WINDOW_GadgetHelp, TRUE,
        WINDOW_NewMenu, (ULONG)amigami_newmenu,
        WINDOW_ParentGroup, gui->ag_Layout,
    EndWindow;

    if (gui->ag_WinObj == NULL) {
        DisposeObject(gui->ag_Layout);
        gui->ag_Layout = NULL;
        return FALSE;
    }

    gui->ag_Window = (struct Window *)RA_OpenWindow(gui->ag_WinObj);
    if (gui->ag_Window == NULL) {
        DisposeObject(gui->ag_WinObj);
        gui->ag_WinObj = NULL;
        gui->ag_Layout = NULL;
        return FALSE;
    }

    gui_init_pens(gui);
    AmigamiGuiRefreshScreenTitle(gui);
    return TRUE;
}

static void
gui_close_main(struct AmigamiGui *gui)
{
    AmigamiSpeedBarDetach(gui);
    AmigamiArticlesFree(gui);
    AmigamiFeedsFree(gui);

    if (gui->ag_WinObj != NULL) {
        if (gui->ag_Window != NULL) {
            RA_CloseWindow(gui->ag_WinObj);
            gui->ag_Window = NULL;
        }
        DisposeObject(gui->ag_WinObj);
        gui->ag_WinObj = NULL;
        gui->ag_Layout = NULL;
        gui->ag_MainRow = NULL;
        gui->ag_FeedsLB = NULL;
        gui->ag_ArticlesLB = NULL;
        gui->ag_Rtb = NULL;
        gui->ag_Scroller = NULL;
        gui->ag_BtnAdd = NULL;
        gui->ag_BtnRemove = NULL;
        gui->ag_BtnRefresh = NULL;
        gui->ag_BtnOpen = NULL;
        gui->ag_SpeedBar = NULL;
    }

    AmigamiSpeedBarFree(gui);
}

static void
gui_handle_menu(struct AmigamiGui *gui, UWORD code)
{
    struct Menu *menu;
    struct MenuItem *item;
    ULONG id;

    menu = NULL;
    GetAttr(WINDOW_MenuStrip, gui->ag_WinObj, (ULONG *)&menu);
    if (menu == NULL) {
        return;
    }

    while (code != MENUNULL) {
        item = ItemAddress(menu, code);
        if (item == NULL) {
            break;
        }
        id = (ULONG)GTMENUITEM_USERDATA(item);
        AmigamiGuiNoteUserActivity(gui);
        switch (id) {
        case MID_QUIT:
            gui->ag_QuitRequested = TRUE;
            break;
        case MID_ADD_FEED:
            AmigamiFeedsAddDialog(gui);
            break;
        case MID_REMOVE_FEED:
            AmigamiFeedsRemoveSelected(gui);
            break;
        case MID_REFRESH:
            AmigamiRefreshSelected(gui);
            break;
        case MID_OPEN:
            AmigamiOpenSelectedLink(gui);
            break;
        case MID_TOGGLE_FEEDS:
            AmigamiGuiToggleFeeds(gui);
            break;
        }
        code = item->NextSelect;
    }
}

static BOOL
gui_handle(struct AmigamiGui *gui)
{
    ULONG result;
    UWORD code;
    BOOL quit;

    quit = FALSE;
    code = 0;
    while ((result = RA_HandleInput(gui->ag_WinObj, &code)) != WMHI_LASTMSG) {
        switch (result & WMHI_CLASSMASK) {
        case WMHI_CLOSEWINDOW:
            quit = TRUE;
            break;
        case WMHI_MENUPICK:
            if ((result & WMHI_MENUMASK) != MENUNULL) {
                gui_handle_menu(gui, (UWORD)(result & WMHI_MENUMASK));
            }
            if (gui->ag_QuitRequested) {
                quit = TRUE;
            }
            break;
        case WMHI_GADGETUP:
            AmigamiGuiNoteUserActivity(gui);
            switch (result & WMHI_GADGETMASK) {
            case GID_BTN_ADD:
                AmigamiFeedsAddDialog(gui);
                break;
            case GID_BTN_REMOVE:
                AmigamiFeedsRemoveSelected(gui);
                break;
            case GID_BTN_REFRESH:
                AmigamiRefreshSelected(gui);
                break;
            case GID_BTN_OPEN:
                AmigamiOpenSelectedLink(gui);
                break;
            case GID_SPEEDBAR:
                AmigamiSpeedBarHandle(gui, code);
                break;
            case GID_FEEDS:
                AmigamiFeedsHandleSelect(gui);
                break;
            case GID_ARTICLES:
                AmigamiArticlesHandleSelect(gui);
                break;
            case GID_PREVIEW:
                {
                    ULONG rel;
                    ULONG kind;

                    rel = 0;
                    kind = 0;
                    GetAttr(RTB_RelEvent, gui->ag_Rtb, &rel);
                    GetAttr(RTB_HitKind, gui->ag_Rtb, &kind);
                    if (rel == RTBE_LINKACTIVATE || kind == RTBH_LINK) {
                        AmigamiOpenSelectedLink(gui);
                    }
                }
                break;
            case GID_SCROLLER:
                AmigamiPreviewFromScroller(gui);
                break;
            }
            break;
        }
    }
    return quit;
}

LONG
AmigamiGuiRun(STRPTR cafile, BOOL insecure, BOOL verbose)
{
    struct AmigamiGui gui;
    BOOL running;
    ULONG signal;

    memset(&gui, 0, sizeof(gui));
    gui.ag_Verbose = verbose;
    gui.ag_Insecure = insecure;
    AmLogInit(verbose);
    if (cafile != NULL) {
        strncpy((char *)gui.ag_CaFile, (char *)cafile,
            sizeof(gui.ag_CaFile) - 1);
    } else {
        strcpy((char *)gui.ag_CaFile, AMIGAMI_DEFAULT_CA);
    }

    if (WindowBase == NULL || LayoutBase == NULL ||
        ButtonBase == NULL || StringBase == NULL ||
        ListBrowserBase == NULL) {
        PutStr("Amigami: ReAction classes missing "
            "(need CLASSES: + lib:reaction.lib)\n");
        return RETURN_FAIL;
    }

    if (HttpBase == NULL) {
        HttpBase = OpenLibrary("amihttp.library", 1);
    }
    if (HttpBase == NULL) {
        PutStr("Amigami: open amihttp.library failed\n");
        return RETURN_FAIL;
    }

    if (RichTextBrowserBase == NULL) {
        RichTextBrowserBase = OpenLibrary("gadgets/richtextbrowser.gadget", 0);
    }
    if (RichTextBrowserBase == NULL) {
        RichTextBrowserBase = OpenLibrary("richtextbrowser.gadget", 0);
    }
    if (RichTextBrowserBase == NULL) {
        PutStr("Amigami: open richtextbrowser.gadget failed\n");
        return RETURN_FAIL;
    }

    AmFeedStoreInit(&gui.ag_Store);
    AmFeedStoreSetPaths(&gui.ag_Store, NULL);
    AmFeedStoreLoad(&gui.ag_Store);
    AmFeedCacheInit();

    if (!AmFetchInit(&gui.ag_Fetch, gui.ag_CaFile, insecure, verbose)) {
        PutStr("Amigami: HTTP session init failed\n");
        AmFeedStoreFree(&gui.ag_Store);
        return RETURN_FAIL;
    }

    if (!gui_open_main(&gui)) {
        PutStr("Amigami: cannot open main window\n");
        AmFetchShutdown(&gui.ag_Fetch);
        AmFeedStoreFree(&gui.ag_Store);
        return RETURN_FAIL;
    }

    AmigamiFeedsRebuild(&gui);
    AmigamiPreviewShowHint(&gui, (STRPTR)"Select Today, All Feeds, or a feed");

    running = TRUE;
    while (running) {
        GetAttr(WINDOW_SigMask, gui.ag_WinObj, &signal);
        Wait(signal | SIGBREAKF_CTRL_C);
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            running = FALSE;
            break;
        }
        if (gui_handle(&gui)) {
            running = FALSE;
        }
    }

    gui_close_main(&gui);
    AmFetchShutdown(&gui.ag_Fetch);
    AmFeedStoreFree(&gui.ag_Store);
    return RETURN_OK;
}
