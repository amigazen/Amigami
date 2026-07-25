/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * datefmt.h - RSS/Atom pubDate / updated display helpers
 */

#ifndef AMIGAMI_DATEFMT_H
#define AMIGAMI_DATEFMT_H

#include <exec/types.h>

/* Compact "25 Jul" from RFC822 or ISO-8601; else truncated ASCII. */
void AmFormatShortDate(STRPTR pubdate, STRPTR out, ULONG outMax);

/* TRUE if pubdate falls on the local calendar day. */
BOOL AmDateIsToday(STRPTR pubdate);

#endif /* AMIGAMI_DATEFMT_H */
