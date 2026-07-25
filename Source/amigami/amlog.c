/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * amlog.c - VERBOSE console logging via Printf
 */

#include <exec/types.h>
#include <proto/dos.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "amlog.h"

static BOOL g_am_verbose = FALSE;

void
AmLogInit(BOOL verbose)
{
    g_am_verbose = verbose ? TRUE : FALSE;
    if (g_am_verbose) {
        Printf("Amigami: VERBOSE logging on\n");
    }
}

BOOL
AmLogIsOn(void)
{
    return g_am_verbose;
}

void
AmLog(STRPTR fmt, ...)
{
    UBYTE buf[512];
    va_list ap;

    if (!g_am_verbose || fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    vsprintf((char *)buf, (char *)fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    Printf("%s", buf);
}

void
AmLogAlways(STRPTR fmt, ...)
{
    UBYTE buf[512];
    va_list ap;

    if (fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    vsprintf((char *)buf, (char *)fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    Printf("%s", buf);
}

void
AmLogHex(STRPTR label, UBYTE *data, ULONG len, ULONG maxShow)
{
    ULONG i;
    ULONG n;
    UBYTE line[80];
    UBYTE ascii[20];
    ULONG col;

    if (!g_am_verbose) {
        return;
    }
    if (label == NULL) {
        label = (STRPTR)"data";
    }
    if (data == NULL) {
        Printf("Amigami: %s: <NULL>\n", label);
        return;
    }
    if (maxShow == 0) {
        maxShow = 64;
    }
    n = len;
    if (n > maxShow) {
        n = maxShow;
    }
    Printf("Amigami: %s len=%ld show=%ld\n", label,
        (long)len, (long)n);

    i = 0;
    while (i < n) {
        col = 0;
        sprintf((char *)line, "  %04lx: ", (unsigned long)i);
        while (col < 16 && i + col < n) {
            UBYTE b;
            UBYTE hex[4];

            b = data[i + col];
            sprintf((char *)hex, "%02lx ", (unsigned long)b);
            strcat((char *)line, (char *)hex);
            if (b >= 0x20 && b < 0x7F) {
                ascii[col] = b;
            } else {
                ascii[col] = '.';
            }
            col++;
        }
        ascii[col] = '\0';
        while (col < 16) {
            strcat((char *)line, "   ");
            col++;
        }
        strcat((char *)line, " |");
        strcat((char *)line, (char *)ascii);
        strcat((char *)line, "|\n");
        Printf("%s", line);
        i += 16;
    }
    if (len > maxShow) {
        Printf("  ... (%ld more bytes)\n", (long)(len - maxShow));
    }
}

void
AmLogStringCheck(STRPTR label, STRPTR s)
{
    ULONG i;
    ULONG len;
    ULONG controls;
    ULONG utf8leads;
    ULONG high;

    if (!g_am_verbose) {
        return;
    }
    if (label == NULL) {
        label = (STRPTR)"str";
    }
    if (s == NULL) {
        Printf("Amigami: %s: <NULL>\n", label);
        return;
    }
    len = strlen((char *)s);
    controls = 0;
    utf8leads = 0;
    high = 0;
    for (i = 0; i < len; i++) {
        UBYTE b;

        b = (UBYTE)s[i];
        if (b < 0x20 && b != '\t' && b != '\n' && b != '\r') {
            controls++;
        }
        if (b >= 0x80) {
            high++;
        }
        /* UTF-8 lead bytes that should not remain after Latin-1 fold */
        if ((b & 0xE0) == 0xC0 || (b & 0xF0) == 0xE0 ||
            (b & 0xF8) == 0xF0) {
            utf8leads++;
        }
    }
    if (controls == 0 && utf8leads == 0) {
        return; /* clean - stay quiet unless corrupt */
    }

    {
        UBYTE preview[64];
        ULONG i;

        for (i = 0; i < 60 && i < len; i++) {
            UBYTE b;

            b = (UBYTE)s[i];
            if (b < 0x20 || b == 0x7F) {
                preview[i] = '.';
            } else {
                preview[i] = b;
            }
        }
        preview[i] = '\0';
        Printf("Amigami: WARN %s len=%ld ctrl=%ld hi=%ld utf8lead=%ld preview=\"%s\"\n",
            label, (long)len, (long)controls,
            (long)high, (long)utf8leads, preview);
    }
    AmLogHex(label, (UBYTE *)s, len, 48);
}
