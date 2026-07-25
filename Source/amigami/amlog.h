/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * amlog.h - Verbose Printf logging (enable with VERBOSE)
 */

#ifndef AMIGAMI_AMLOG_H
#define AMIGAMI_AMLOG_H

#include <exec/types.h>

void AmLogInit(BOOL verbose);
BOOL AmLogIsOn(void);

/* Printf-style to console when VERBOSE; always NUL-safe. */
void AmLog(STRPTR fmt, ...);

/* Always prints (errors). */
void AmLogAlways(STRPTR fmt, ...);

/*
 * Hex + ASCII dump of up to maxShow bytes (default 64 if maxShow==0).
 * Flags control chars / high UTF-8 lead bytes for listview debugging.
 */
void AmLogHex(STRPTR label, UBYTE *data, ULONG len, ULONG maxShow);

/* Log whether a string still looks like UTF-8 / has controls. */
void AmLogStringCheck(STRPTR label, STRPTR s);

#endif /* AMIGAMI_AMLOG_H */
