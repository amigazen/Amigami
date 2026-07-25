/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * utf8fold.h - UTF-8 → Amiga Latin-1 (ISO-8859-1-ish) conversion
 */

#ifndef AMIGAMI_UTF8FOLD_H
#define AMIGAMI_UTF8FOLD_H

#include <exec/types.h>

ULONG Utf8ToAmigaDisplay(UBYTE *dst, ULONG dstmax, STRPTR src);
ULONG Utf8ToAmigaDisplayLen(UBYTE *dst, ULONG dstmax, UBYTE *src, long srclen);

/*
 * Allocate Latin-1 copy of UTF-8 src (srclen < 0 → strlen).
 * Decodes &#...; entities, strips XML tags, trims junk. Caller FreeVec.
 */
UBYTE *Utf8DupLatin1(UBYTE *src, long srclen);

void AmSanitizeLatin1(UBYTE *s);

/* In-place cleanup for listbrowser / title fields already in memory. */
void AmCleanupLatin1Field(UBYTE *s);

/*
 * Copy already-folded Latin-1 into dst (no second UTF-8 pass - that mangles
 * high bytes like 0xE2 as if they were UTF-8 leads).
 */
ULONG AmLatin1Copy(UBYTE *dst, ULONG dstmax, STRPTR src);

#endif /* AMIGAMI_UTF8FOLD_H */
