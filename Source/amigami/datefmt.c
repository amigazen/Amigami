/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * datefmt.c - Normalize RSS RFC822 and Atom ISO-8601 dates for the UI
 */

#include <exec/types.h>
#include <dos/datetime.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>

#include "datefmt.h"
#include "utf8fold.h"

static const char * const month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Fill day/mon (0-based month) from RFC822-ish or ISO-8601; return TRUE. */
static BOOL
parse_day_mon(STRPTR pubdate, LONG *dayOut, LONG *monOut)
{
    STRPTR p;
    LONG day;
    LONG mon;
    LONG i;
    LONG y;
    LONG m;
    LONG d;

    if (pubdate == NULL || dayOut == NULL || monOut == NULL) {
        return FALSE;
    }

    /*
     * Atom / ISO-8601: 2026-07-24T05:47:42-04:00 or 2026-07-24T05:47:42Z
     * Take the leading YYYY-MM-DD.
     */
    if (pubdate[0] >= '0' && pubdate[0] <= '9' &&
        pubdate[1] >= '0' && pubdate[1] <= '9' &&
        pubdate[2] >= '0' && pubdate[2] <= '9' &&
        pubdate[3] >= '0' && pubdate[3] <= '9' &&
        pubdate[4] == '-' &&
        pubdate[5] >= '0' && pubdate[5] <= '9' &&
        pubdate[6] >= '0' && pubdate[6] <= '9' &&
        pubdate[7] == '-' &&
        pubdate[8] >= '0' && pubdate[8] <= '9' &&
        pubdate[9] >= '0' && pubdate[9] <= '9') {
        y = (pubdate[0] - '0') * 1000 + (pubdate[1] - '0') * 100 +
            (pubdate[2] - '0') * 10 + (pubdate[3] - '0');
        m = (pubdate[5] - '0') * 10 + (pubdate[6] - '0');
        d = (pubdate[8] - '0') * 10 + (pubdate[9] - '0');
        (void)y;
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
            *dayOut = d;
            *monOut = m - 1;
            return TRUE;
        }
    }

    /* RFC822: "Fri, 24 Jul 2026 20:11:37 +0000" */
    day = 0;
    mon = -1;
    p = pubdate;
    while (*p != '\0') {
        if (*p >= '0' && *p <= '9' && day == 0) {
            day = *p - '0';
            if (p[1] >= '0' && p[1] <= '9') {
                day = day * 10 + (p[1] - '0');
            }
        }
        for (i = 0; i < 12; i++) {
            if (strnicmp(p, (STRPTR)month_names[i], 3) == 0) {
                mon = i;
                break;
            }
        }
        if (day > 0 && mon >= 0) {
            break;
        }
        p++;
    }
    if (day > 0 && mon >= 0) {
        *dayOut = day;
        *monOut = mon;
        return TRUE;
    }
    return FALSE;
}

void
AmFormatShortDate(STRPTR pubdate, STRPTR out, ULONG outMax)
{
    LONG day;
    LONG mon;

    if (out == NULL || outMax == 0) {
        return;
    }
    out[0] = '\0';
    if (pubdate == NULL || outMax < 8) {
        return;
    }

    day = 0;
    mon = -1;
    if (parse_day_mon(pubdate, &day, &mon)) {
        sprintf((char *)out, "%ld %s", (long)day, month_names[mon]);
        return;
    }

    AmLatin1Copy(out, outMax, pubdate);
    if (strlen((char *)out) > 12) {
        out[12] = '\0';
    }
}

BOOL
AmDateIsToday(STRPTR pubdate)
{
    struct DateTime dt;
    struct DateStamp ds;
    UBYTE dateStr[40];
    LONG day;
    LONG mon;
    LONG todayDay;
    LONG todayMon;
    LONG i;

    if (pubdate == NULL || pubdate[0] == '\0') {
        return FALSE;
    }
    if (!parse_day_mon(pubdate, &day, &mon)) {
        return FALSE;
    }

    DateStamp(&ds);
    memset(&dt, 0, sizeof(dt));
    dt.dat_Stamp = ds;
    dt.dat_Format = FORMAT_DOS;
    dt.dat_StrDate = (STRPTR)dateStr;
    dateStr[0] = '\0';
    if (!DateToStr(&dt)) {
        return FALSE;
    }
    /* FORMAT_DOS -> "dd-mmm-yy" */
    if (strlen((char *)dateStr) < 6) {
        return FALSE;
    }

    todayDay = 0;
    if (dateStr[0] >= '0' && dateStr[0] <= '9') {
        todayDay = dateStr[0] - '0';
        if (dateStr[1] >= '0' && dateStr[1] <= '9') {
            todayDay = todayDay * 10 + (dateStr[1] - '0');
        }
    }
    todayMon = -1;
    for (i = 0; i < 12; i++) {
        if (strnicmp((STRPTR)&dateStr[3], (STRPTR)month_names[i], 3) == 0) {
            todayMon = i;
            break;
        }
    }
    if (todayDay <= 0 || todayMon < 0) {
        return FALSE;
    }
    if (day == todayDay && mon == todayMon) {
        return TRUE;
    }
    return FALSE;
}
