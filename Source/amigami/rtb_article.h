/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_article.h - FeedItem → richtextbrowser document
 */

#ifndef AMIGAMI_RTB_ARTICLE_H
#define AMIGAMI_RTB_ARTICLE_H

#include <exec/types.h>

/* Forward decls only when full structs are not already visible (SAS/C W63). */
#ifndef AMIGAMI_GUI_H
struct AmigamiGui;
#endif
#ifndef AMIGAMI_RSS_H
struct FeedItem;
#endif

void AmigamiPreviewClear(struct AmigamiGui *gui);
void AmigamiPreviewShowHint(struct AmigamiGui *gui, STRPTR hint);
BOOL AmigamiPreviewShowItem(struct AmigamiGui *gui, struct FeedItem *item);
void AmigamiPreviewSyncScroller(struct AmigamiGui *gui);
void AmigamiPreviewFromScroller(struct AmigamiGui *gui);

/* Strip simple HTML tags into dst (NUL-terminated). */
void AmStripHtml(UBYTE *dst, ULONG dstmax, STRPTR src);

#endif /* AMIGAMI_RTB_ARTICLE_H */
