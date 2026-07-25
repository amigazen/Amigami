/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_article.c - Map FeedItem into richtextbrowser preview
 *
 * Pulls HN-style "Article URL:" / "Comments URL:" / Points footers out of
 * the body into proper link/meta runs instead of dumping them as prose.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/alib.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"
#include "rss.h"
#include "utf8fold.h"
#include "datefmt.h"
#include "rtb_article.h"

void
AmStripHtml(UBYTE *dst, ULONG dstmax, STRPTR src)
{
    UBYTE *d;
    ULONG left;
    BOOL inTag;

    if (dst == NULL || dstmax == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    d = dst;
    left = dstmax;
    inTag = FALSE;

    while (*src != '\0' && left > 1) {
        UBYTE ch;

        ch = (UBYTE)*src++;
        if (inTag) {
            if (ch == (UBYTE)'>') {
                inTag = FALSE;
            }
            continue;
        }
        if (ch == (UBYTE)'<') {
            /* Turn block breaks into paragraph separators */
            if (strnicmp((char *)src, "br", 2) == 0 ||
                strnicmp((char *)src, "p", 1) == 0 ||
                strnicmp((char *)src, "/p", 2) == 0) {
                if (d > dst && d[-1] != '\n' && left > 2) {
                    *d++ = '\n';
                    left--;
                }
            }
            inTag = TRUE;
            continue;
        }
        if (ch == (UBYTE)'&') {
            if (src[0] == 'l' && src[1] == 't' && src[2] == ';') {
                src += 3;
                ch = '<';
            } else if (src[0] == 'g' && src[1] == 't' && src[2] == ';') {
                src += 3;
                ch = '>';
            } else if (src[0] == 'a' && src[1] == 'm' && src[2] == 'p' &&
                src[3] == ';') {
                src += 4;
                ch = '&';
            } else if (src[0] == 'q' && src[1] == 'u' && src[2] == 'o' &&
                src[3] == 't' && src[4] == ';') {
                src += 5;
                ch = '"';
            } else if (src[0] == 'n' && src[1] == 'b' && src[2] == 's' &&
                src[3] == 'p' && src[4] == ';') {
                src += 5;
                ch = ' ';
            } else {
                /* Keep raw &#...; for AmCleanupLatin1Field after strip. */
                *d++ = ch;
                left--;
                continue;
            }
        }
        if (ch == (UBYTE)'\r') {
            continue;
        }
        if (ch == (UBYTE)'\t') {
            ch = ' ';
        }
        *d++ = ch;
        left--;
    }
    *d = '\0';
}

void
AmigamiPreviewClear(struct AmigamiGui *gui)
{
    if (gui == NULL || gui->ag_Rtb == NULL) {
        return;
    }
    DoMethod(gui->ag_Rtb, RTBM_CLEAR, NULL);
}

void
AmigamiPreviewShowHint(struct AmigamiGui *gui, STRPTR hint)
{
    struct RtbBlock *blk;
    struct RtbRun *run;
    WORD fg;

    if (gui == NULL || gui->ag_Rtb == NULL) {
        return;
    }
    if (hint == NULL) {
        hint = (STRPTR)"Select an article";
    }
    fg = gui->ag_PenFg;

    SetGadgetAttrs((struct Gadget *)gui->ag_Rtb, gui->ag_Window, NULL,
        RTB_Busy, TRUE, TAG_DONE);
    DoMethod(gui->ag_Rtb, RTBM_CLEAR, NULL);

    blk = AllocRtbBlockTags(RTBB_PARAGRAPH, RTBA_FgPen, (ULONG)fg, TAG_DONE);
    if (blk != NULL) {
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)hint,
            RTBA_FgPen, (ULONG)fg,
            RTBA_FontName, (ULONG)"Helvetica",
            RTBA_Size, 11,
            RTBA_Style, RTBS_ITALIC,
            TAG_DONE);
        if (run != NULL) {
            RtbBlockAddRun(blk, run);
        }
        DoMethod(gui->ag_Rtb, RTBM_INSERTBLOCK, NULL, blk, 0);
    }

    SetGadgetAttrs((struct Gadget *)gui->ag_Rtb, gui->ag_Window, NULL,
        RTB_Busy, FALSE, TAG_DONE);
    AmigamiPreviewSyncScroller(gui);
}

void
AmigamiPreviewSyncScroller(struct AmigamiGui *gui)
{
    LONG top;
    LONG total;
    LONG visible;
    LONG curTop;
    LONG curTotal;
    LONG curVis;

    if (gui == NULL || gui->ag_Rtb == NULL || gui->ag_Scroller == NULL) {
        return;
    }
    top = 0;
    total = 0;
    visible = 0;
    curTop = 0;
    curTotal = 0;
    curVis = 0;
    GetAttr(RTB_Top, gui->ag_Rtb, (ULONG *)&top);
    GetAttr(RTB_Total, gui->ag_Rtb, (ULONG *)&total);
    GetAttr(RTB_Visible, gui->ag_Rtb, (ULONG *)&visible);
    GetAttr(SCROLLER_Top, gui->ag_Scroller, (ULONG *)&curTop);
    GetAttr(SCROLLER_Total, gui->ag_Scroller, (ULONG *)&curTotal);
    GetAttr(SCROLLER_Visible, gui->ag_Scroller, (ULONG *)&curVis);

    if (total < 1) {
        total = 1;
    }
    if (visible < 1) {
        visible = 1;
    }

    if (top == curTop && total == curTotal && visible == curVis) {
        return;
    }

    SetGadgetAttrs((struct Gadget *)gui->ag_Scroller, gui->ag_Window, NULL,
        SCROLLER_Top, top,
        SCROLLER_Total, total,
        SCROLLER_Visible, visible,
        TAG_DONE);
}

void
AmigamiPreviewFromScroller(struct AmigamiGui *gui)
{
    LONG top;
    LONG cur;

    if (gui == NULL || gui->ag_Rtb == NULL || gui->ag_Scroller == NULL) {
        return;
    }
    top = 0;
    cur = 0;
    GetAttr(SCROLLER_Top, gui->ag_Scroller, (ULONG *)&top);
    GetAttr(RTB_Top, gui->ag_Rtb, (ULONG *)&cur);
    if (top == cur) {
        return;
    }
    SetGadgetAttrs((struct Gadget *)gui->ag_Rtb, gui->ag_Window, NULL,
        RTB_Top, top,
        TAG_DONE);
}

static BOOL
add_text_run(struct RtbBlock *para, STRPTR text, ULONG style, WORD pen,
    UWORD size)
{
    struct RtbRun *run;

    if (para == NULL || text == NULL || text[0] == '\0') {
        return FALSE;
    }
    run = AllocRtbRunTags(RTBR_TEXT,
        RTBA_Text, (ULONG)text,
        RTBA_Style, style,
        RTBA_FgPen, (ULONG)pen,
        RTBA_FontName, (ULONG)"Helvetica",
        RTBA_Size, size,
        TAG_DONE);
    if (run == NULL) {
        return FALSE;
    }
    if (!RtbBlockAddRun(para, run)) {
        FreeRtbRun(run);
        return FALSE;
    }
    return TRUE;
}

static BOOL
add_link_run(struct RtbBlock *para, STRPTR href, STRPTR text, WORD pen)
{
    struct RtbRun *run;

    if (para == NULL || href == NULL || href[0] == '\0') {
        return FALSE;
    }
    if (text == NULL || text[0] == '\0') {
        text = href;
    }
    run = AllocRtbRunTags(RTBR_LINK,
        RTBA_Href, (ULONG)href,
        RTBA_Text, (ULONG)text,
        RTBA_FgPen, (ULONG)pen,
        RTBA_FontName, (ULONG)"Helvetica",
        RTBA_Size, 11,
        TAG_DONE);
    if (run == NULL) {
        return FALSE;
    }
    if (!RtbBlockAddRun(para, run)) {
        FreeRtbRun(run);
        return FALSE;
    }
    return TRUE;
}

static BOOL
insert_block(struct AmigamiGui *gui, struct RtbBlock *blk)
{
    if (gui == NULL || gui->ag_Rtb == NULL || blk == NULL) {
        return FALSE;
    }
    DoMethod(gui->ag_Rtb, RTBM_INSERTBLOCK, NULL, blk, 0);
    return TRUE;
}

static BOOL
insert_para(struct AmigamiGui *gui, STRPTR text, ULONG style, WORD pen,
    UWORD size)
{
    struct RtbBlock *blk;

    if (text == NULL || text[0] == '\0') {
        return FALSE;
    }
    blk = AllocRtbBlockTags(RTBB_PARAGRAPH, RTBA_FgPen, (ULONG)pen, TAG_DONE);
    if (blk == NULL) {
        return FALSE;
    }
    if (!add_text_run(blk, text, style, pen, size)) {
        FreeRtbBlock(blk);
        return FALSE;
    }
    return insert_block(gui, blk);
}

/*
 * Peel HN/rss bridge footers into structured fields.
 * Lines like "Article URL: ..." / "Comments URL: ..." / "Points: ...".
 */
static void
peel_feed_extras(UBYTE *body, UBYTE *articleUrl, ULONG articleMax,
    UBYTE *commentsUrl, ULONG commentsMax, UBYTE *points, ULONG pointsMax,
    UBYTE *nComments, ULONG nCommentsMax)
{
    UBYTE *r;
    UBYTE *w;
    UBYTE line[512];
    ULONG li;

    if (articleUrl != NULL && articleMax > 0) {
        articleUrl[0] = '\0';
    }
    if (commentsUrl != NULL && commentsMax > 0) {
        commentsUrl[0] = '\0';
    }
    if (points != NULL && pointsMax > 0) {
        points[0] = '\0';
    }
    if (nComments != NULL && nCommentsMax > 0) {
        nComments[0] = '\0';
    }
    if (body == NULL) {
        return;
    }

    r = body;
    w = body;
    while (*r != '\0') {
        li = 0;
        while (*r != '\0' && *r != '\n' && li + 1 < sizeof(line)) {
            line[li++] = *r++;
        }
        line[li] = '\0';
        if (*r == '\n') {
            r++;
        }

        if (strnicmp((char *)line, "Article URL:", 12) == 0) {
            UBYTE *p;

            p = line + 12;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (articleUrl != NULL && articleMax > 1 && *p != '\0') {
                strncpy((char *)articleUrl, (char *)p, articleMax - 1);
                articleUrl[articleMax - 1] = '\0';
            }
            continue;
        }
        if (strnicmp((char *)line, "Comments URL:", 13) == 0) {
            UBYTE *p;

            p = line + 13;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (commentsUrl != NULL && commentsMax > 1 && *p != '\0') {
                strncpy((char *)commentsUrl, (char *)p, commentsMax - 1);
                commentsUrl[commentsMax - 1] = '\0';
            }
            continue;
        }
        if (strnicmp((char *)line, "Points:", 7) == 0) {
            UBYTE *p;

            p = line + 7;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (points != NULL && pointsMax > 1 && *p != '\0') {
                strncpy((char *)points, (char *)p, pointsMax - 1);
                points[pointsMax - 1] = '\0';
            }
            continue;
        }
        if (strnicmp((char *)line, "# Comments:", 11) == 0 ||
            strnicmp((char *)line, "Comments:", 9) == 0) {
            UBYTE *p;

            p = line;
            while (*p != '\0' && *p != ':') {
                p++;
            }
            if (*p == ':') {
                p++;
            }
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (nComments != NULL && nCommentsMax > 1 && *p != '\0') {
                strncpy((char *)nComments, (char *)p, nCommentsMax - 1);
                nComments[nCommentsMax - 1] = '\0';
            }
            continue;
        }

        /* Keep ordinary body lines */
        {
            UBYTE *s;

            s = line;
            while (*s != '\0') {
                *w++ = *s++;
            }
            *w++ = '\n';
        }
    }
    *w = '\0';
}

/* Insert body as one paragraph per blank-line / newline group. */
static void
insert_body_paragraphs(struct AmigamiGui *gui, UBYTE *body, WORD fg)
{
    UBYTE *p;
    UBYTE *start;
    UBYTE chunk[1024];
    ULONG n;

    if (body == NULL || body[0] == '\0') {
        insert_para(gui, (STRPTR)"(no content)", 0, fg, 12);
        return;
    }

    p = body;
    while (*p != '\0') {
        while (*p == '\n' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && !(*p == '\n' && (p[1] == '\n' || p[1] == '\0'))) {
            if (*p == '\n') {
                /* soft line break inside a paragraph -> space */
                *p = ' ';
            }
            p++;
        }
        n = (ULONG)(p - start);
        if (n >= sizeof(chunk)) {
            n = sizeof(chunk) - 1;
        }
        if (n > 0) {
            CopyMem(start, chunk, n);
            chunk[n] = '\0';
            AmCleanupLatin1Field(chunk);
            insert_para(gui, (STRPTR)chunk, 0, fg, 12);
        }
        if (*p == '\n') {
            p++;
            if (*p == '\n') {
                p++;
            }
        }
    }
}

BOOL
AmigamiPreviewShowItem(struct AmigamiGui *gui, struct FeedItem *item)
{
    struct RtbBlock *titleBlk;
    struct RtbBlock *linkBlk;
    struct RtbBlock *rule;
    UBYTE foldBuf[4096];
    UBYTE plainBuf[4096];
    UBYTE metaBuf[256];
    UBYTE shortDate[24];
    UBYTE articleUrl[512];
    UBYTE commentsUrl[512];
    UBYTE points[32];
    UBYTE nComments[32];
    WORD fg;
    WORD linkPen;
    STRPTR primaryLink;

    if (gui == NULL || gui->ag_Rtb == NULL || item == NULL) {
        return FALSE;
    }

    fg = gui->ag_PenFg;
    linkPen = gui->ag_PenFill;
    primaryLink = item->link;

    SetGadgetAttrs((struct Gadget *)gui->ag_Rtb, gui->ag_Window, NULL,
        RTB_Busy, TRUE,
        TAG_DONE);
    DoMethod(gui->ag_Rtb, RTBM_CLEAR, NULL);

    /* Title - already Latin-1 from parse; do not UTF-8-fold again. */
    foldBuf[0] = '\0';
    if (item->title != NULL) {
        AmLatin1Copy(foldBuf, sizeof(foldBuf), (STRPTR)item->title);
    } else {
        strcpy((char *)foldBuf, "(untitled)");
    }
    titleBlk = AllocRtbBlockTags(RTBB_HEADING,
        RTBA_FgPen, (ULONG)fg,
        TAG_DONE);
    if (titleBlk != NULL) {
        add_text_run(titleBlk, (STRPTR)foldBuf, RTBS_BOLD, fg, 14);
        insert_block(gui, titleBlk);
    }

    /* Author / date meta line */
    metaBuf[0] = '\0';
    shortDate[0] = '\0';
    AmFormatShortDate(item->pubdate, shortDate, sizeof(shortDate));
    {
        UBYTE aFold[160];

        aFold[0] = '\0';
        if (item->author != NULL) {
            AmLatin1Copy(aFold, sizeof(aFold), (STRPTR)item->author);
        }
        if (aFold[0] != '\0' && shortDate[0] != '\0') {
            sprintf((char *)metaBuf, "%s - %s", (char *)aFold,
                (char *)shortDate);
        } else if (aFold[0] != '\0') {
            strcpy((char *)metaBuf, (char *)aFold);
        } else if (shortDate[0] != '\0') {
            strcpy((char *)metaBuf, (char *)shortDate);
        } else if (item->pubdate != NULL) {
            AmLatin1Copy(metaBuf, sizeof(metaBuf), (STRPTR)item->pubdate);
        }
    }
    if (metaBuf[0] != '\0') {
        insert_para(gui, (STRPTR)metaBuf, RTBS_ITALIC, fg, 10);
    }

    /* Body + peel optional HN-style trailer fields */
    plainBuf[0] = '\0';
    articleUrl[0] = '\0';
    commentsUrl[0] = '\0';
    points[0] = '\0';
    nComments[0] = '\0';
    if (item->description != NULL) {
        AmStripHtml(plainBuf, sizeof(plainBuf), (STRPTR)item->description);
        AmCleanupLatin1Field(plainBuf);
        peel_feed_extras(plainBuf, articleUrl, sizeof(articleUrl),
            commentsUrl, sizeof(commentsUrl), points, sizeof(points),
            nComments, sizeof(nComments));
    }

    if (articleUrl[0] != '\0' &&
        (primaryLink == NULL || primaryLink[0] == '\0')) {
        primaryLink = (STRPTR)articleUrl;
    }

    /* Score / comment count when present */
    if (points[0] != '\0' || nComments[0] != '\0') {
        UBYTE score[80];

        score[0] = '\0';
        if (points[0] != '\0' && nComments[0] != '\0') {
            sprintf((char *)score, "%s points - %s comments",
                (char *)points, (char *)nComments);
        } else if (points[0] != '\0') {
            sprintf((char *)score, "%s points", (char *)points);
        } else {
            sprintf((char *)score, "%s comments", (char *)nComments);
        }
        insert_para(gui, (STRPTR)score, 0, fg, 10);
    }

    /* Primary + comments links */
    if (primaryLink != NULL && primaryLink[0] != '\0') {
        linkBlk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
        if (linkBlk != NULL) {
            add_link_run(linkBlk, primaryLink, (STRPTR)"Open article",
                linkPen);
            insert_block(gui, linkBlk);
        }
    }
    if (commentsUrl[0] != '\0') {
        linkBlk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
        if (linkBlk != NULL) {
            add_link_run(linkBlk, (STRPTR)commentsUrl,
                (STRPTR)"View comments", linkPen);
            insert_block(gui, linkBlk);
        }
    }

    rule = AllocRtbBlockTags(RTBB_RULE, RTBA_Thickness, 1, TAG_DONE);
    if (rule != NULL) {
        insert_block(gui, rule);
    }

    insert_body_paragraphs(gui, plainBuf, fg);

    SetGadgetAttrs((struct Gadget *)gui->ag_Rtb, gui->ag_Window, NULL,
        RTB_Busy, FALSE,
        TAG_DONE);
    AmigamiPreviewSyncScroller(gui);
    return TRUE;
}
