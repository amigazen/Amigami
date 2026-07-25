/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * utf8fold.c - Port of AWeb Translate() UTF-8 display fold (no TTEngine,
 * no HTML entities). Source: AWeb3/Source/AWebAPL/parse.c Translate().
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "utf8fold.h"

/* Amigami copy of Seiten utf8fold (AWeb Translate cheats). */

/* Latin Extended-A/B strip tables (trail - 0x80 indexes 0..63). */
static const UBYTE latin_ext_a_c4[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiiiJjKkkLlLlLlL";
static const UBYTE latin_ext_a_c5[] =
    "lLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
static const UBYTE latin_ext_b_c6[] =
    "bBBbbbCCcDDDddEaEFfGGhIIKkllWNnOCoOoPpRS2EetTttUuUVYyZzEEee255?w";
static const UBYTE latin_ext_b_c7[] =
    "||||DDdLLlNNnAaIiOoUuUuUuUuUueAaAaAaGgGgKkOoOoEejDDdGgHWNnAaAaOo";

static void
fold_putc(UBYTE **dp, ULONG *left, UBYTE ch)
{
    if (*left > 1) {
        **dp = ch;
        (*dp)++;
        (*left)--;
    }
}

static void
fold_puts(UBYTE **dp, ULONG *left, STRPTR s)
{
    while (s != NULL && *s != '\0' && *left > 1) {
        **dp = (UBYTE)*s;
        (*dp)++;
        (*left)--;
        s++;
    }
}

ULONG
Utf8ToAmigaDisplayLen(UBYTE *dst, ULONG dstmax, UBYTE *src, long srclen)
{
    UBYTE *p;
    UBYTE *end;
    UBYTE *d;
    ULONG left;
    ULONG written;

    if (dst == NULL || dstmax == 0) {
        return 0;
    }
    dst[0] = '\0';
    if (src == NULL || srclen <= 0) {
        return 0;
    }

    p = src;
    end = src + srclen;
    d = dst;
    left = dstmax;

    while (p < end && left > 1) {
        ULONG utf8_bytes;
        UBYTE replacement;
        STRPTR replacement_str;
        BOOL skip_char;
        ULONG utf8_char;

        utf8_bytes = 0;
        replacement = 0;
        replacement_str = NULL;
        skip_char = FALSE;
        utf8_char = 0;

        /* 4-byte UTF-8 (emoji / non-BMP) -> middle-dot */
        if ((*p & 0xF8) == 0xF0 && p + 3 < end &&
            (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
            (p[3] & 0xC0) == 0x80) {
            utf8_bytes = 4;
            replacement = '?';
        }
        /* 3-byte */
        else if ((*p & 0xF0) == 0xE0 && p + 2 < end &&
            (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            utf8_bytes = 3;
            utf8_char = ((ULONG)(*p & 0x0F) << 12) |
                ((ULONG)(p[1] & 0x3F) << 6) |
                (ULONG)(p[2] & 0x3F);

            if (*p == 0xE2 && p[1] == 0x80) {
                switch (p[2]) {
                case 0x8A:
                    replacement = 0x20;
                    break;
                case 0x93:
                    replacement = 0x2D;
                    break;
                case 0x94:
                    replacement_str = (STRPTR)"--";
                    break;
                case 0x98:
                    replacement = 0x60;
                    break;
                case 0x99:
                    replacement = 0x27;
                    break;
                case 0x9C:
                case 0x9D:
                    replacement = 0x22;
                    break;
                case 0xA6:
                    replacement_str = (STRPTR)"...";
                    break;
                case 0xAF:
                    replacement = 0x20;
                    break;
                default:
                    replacement = '?';
                    break;
                }
            } else if (*p == 0xE2 && p[1] == 0x96) {
                if (p[2] == 0x88) {
                    replacement = '*';
                } else {
                    replacement = '?';
                }
            } else {
                switch (utf8_char) {
                case 0x2018:
                    replacement = 0x60;
                    break;
                case 0x2019:
                case 0x201A:
                    replacement = 0x27;
                    break;
                case 0x201C:
                case 0x201D:
                case 0x201E:
                    replacement = 0x22;
                    break;
                case 0x2013:
                    replacement = 0x2D;
                    break;
                case 0x2014:
                    replacement_str = (STRPTR)"--";
                    break;
                case 0x2022:
                    replacement = '*';
                    break;
                case 0x2026:
                    replacement_str = (STRPTR)"...";
                    break;
                case 0x2039:
                    replacement = '<';
                    break;
                case 0x203A:
                    replacement = '>';
                    break;
                case 0x2122:
                    replacement_str = (STRPTR)"TM";
                    break;
                case 0x00A0:
                    replacement = 0x20;
                    break;
                case 0x00A9:
                    replacement = 0xA9;
                    break;
                case 0x00AE:
                    replacement = 0xAE;
                    break;
                default:
                    if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                        replacement = (UBYTE)utf8_char;
                    } else {
                        replacement = '?';
                    }
                    break;
                }
            }
        }
        /* 2-byte */
        else if ((*p & 0xE0) == 0xC0 && *p >= 0xC2 && p + 1 < end &&
            (p[1] & 0xC0) == 0x80) {
            utf8_bytes = 2;
            utf8_char = ((ULONG)(*p & 0x1F) << 6) | (ULONG)(p[1] & 0x3F);

            if (*p == 0xC2) {
                if (p[1] == 0xAD) {
                    skip_char = TRUE;
                } else if (p[1] == 0xA0) {
                    replacement = 0x20;
                } else if (p[1] >= 0xA0) {
                    replacement = p[1];
                } else if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                    replacement = (UBYTE)utf8_char;
                } else {
                    replacement = '?';
                }
            } else if (*p == 0xC3) {
                replacement = (UBYTE)(p[1] + 0x40);
            } else if (*p == 0xC4) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_a_c4[p[1] - 0x80];
                } else {
                    replacement = '?';
                }
            } else if (*p == 0xC5) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_a_c5[p[1] - 0x80];
                } else {
                    replacement = '?';
                }
            } else if (*p == 0xC6) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_b_c6[p[1] - 0x80];
                } else {
                    replacement = '?';
                }
            } else if (*p == 0xC7) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_b_c7[p[1] - 0x80];
                } else {
                    replacement = '?';
                }
            } else if (*p == 0xCC || (*p == 0xCD && p[1] <= 0xAF)) {
                skip_char = TRUE;
            } else if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                replacement = (UBYTE)utf8_char;
            } else {
                replacement = '?';
            }
        }

        if (utf8_bytes > 0) {
            if (skip_char) {
                /* consume only */
            } else if (replacement_str != NULL) {
                fold_puts(&d, &left, replacement_str);
            } else if (replacement > 0) {
                fold_putc(&d, &left, replacement);
            } else {
                fold_putc(&d, &left, *p);
            }
            p += utf8_bytes;
        } else {
            /*
             * ASCII, already-Latin-1, or incomplete/invalid UTF-8 lead.
             * Copy one byte so pre-folded Latin-1 survives a second pass.
             */
            fold_putc(&d, &left, *p);
            p++;
        }
    }

    *d = '\0';
    written = (ULONG)(d - dst);
    return written;
}

ULONG
Utf8ToAmigaDisplay(UBYTE *dst, ULONG dstmax, STRPTR src)
{
    if (src == NULL) {
        if (dst != NULL && dstmax > 0) {
            dst[0] = '\0';
        }
        return 0;
    }
    return Utf8ToAmigaDisplayLen(dst, dstmax, (UBYTE *)src,
        (long)strlen((char *)src));
}

void
AmSanitizeLatin1(UBYTE *s)
{
    UBYTE *p;

    if (s == NULL) {
        return;
    }
    for (p = s; *p != '\0'; p++) {
        if (*p < 0x20 && *p != '\t' && *p != '\n' && *p != '\r') {
            *p = ' ';
        } else if (*p == 0x7F) {
            *p = ' ';
        }
    }
}

/* Decode &#NNN; &#xHH; and a few named entities in place (shortens string). */
static void
decode_entities(UBYTE *s)
{
    UBYTE *r;
    UBYTE *w;

    if (s == NULL) {
        return;
    }
    r = s;
    w = s;
    while (*r != '\0') {
        if (r[0] == '&') {
            if (r[1] == '#') {
                ULONG code;
                UBYTE *p;

                code = 0;
                p = r + 2;
                if (*p == 'x' || *p == 'X') {
                    p++;
                    while ((*p >= '0' && *p <= '9') ||
                        (*p >= 'a' && *p <= 'f') ||
                        (*p >= 'A' && *p <= 'F')) {
                        code <<= 4;
                        if (*p >= '0' && *p <= '9') {
                            code += (ULONG)(*p - '0');
                        } else if (*p >= 'a' && *p <= 'f') {
                            code += (ULONG)(*p - 'a' + 10);
                        } else {
                            code += (ULONG)(*p - 'A' + 10);
                        }
                        p++;
                    }
                } else {
                    while (*p >= '0' && *p <= '9') {
                        code = code * 10UL + (ULONG)(*p - '0');
                        p++;
                    }
                }
                if (*p == ';') {
                    p++;
                    if (code == 0) {
                        *w++ = ' ';
                    } else if (code < 0x20 && code != '\t' &&
                        code != '\n' && code != '\r') {
                        *w++ = ' ';
                    } else if (code <= 255UL) {
                        *w++ = (UBYTE)code;
                    } else {
                        /* RSS often uses Unicode entities; map to ASCII. */
                        switch (code) {
                        case 0x2018UL: /* &#8216; left single */
                        case 0x2019UL: /* &#8217; right single */
                        case 0x201AUL:
                            *w++ = '\'';
                            break;
                        case 0x201CUL: /* &#8220; */
                        case 0x201DUL: /* &#8221; */
                        case 0x201EUL:
                            *w++ = '"';
                            break;
                        case 0x2013UL: /* &#8211; en-dash */
                            *w++ = '-';
                            break;
                        case 0x2014UL: /* &#8212; em-dash */
                            *w++ = '-';
                            *w++ = '-';
                            break;
                        case 0x2022UL: /* &#8226; bullet */
                            *w++ = '*';
                            break;
                        case 0x2026UL: /* &#8230; ellipsis */
                            *w++ = '.';
                            *w++ = '.';
                            *w++ = '.';
                            break;
                        default:
                            *w++ = '?';
                            break;
                        }
                    }
                    r = p;
                    continue;
                }
            } else if (strnicmp((char *)r, "&amp;", 5) == 0) {
                *w++ = '&';
                r += 5;
                continue;
            } else if (strnicmp((char *)r, "&lt;", 4) == 0) {
                *w++ = '<';
                r += 4;
                continue;
            } else if (strnicmp((char *)r, "&gt;", 4) == 0) {
                *w++ = '>';
                r += 4;
                continue;
            } else if (strnicmp((char *)r, "&quot;", 6) == 0) {
                *w++ = '"';
                r += 6;
                continue;
            } else if (strnicmp((char *)r, "&apos;", 6) == 0) {
                *w++ = '\'';
                r += 6;
                continue;
            } else if (strnicmp((char *)r, "&nbsp;", 6) == 0) {
                *w++ = ' ';
                r += 6;
                continue;
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

/* Remove <tags>, keep text. */
static void
strip_xml_tags(UBYTE *s)
{
    UBYTE *r;
    UBYTE *w;
    BOOL inTag;

    if (s == NULL) {
        return;
    }
    r = s;
    w = s;
    inTag = FALSE;
    while (*r != '\0') {
        if (!inTag && *r == '<') {
            inTag = TRUE;
            r++;
            continue;
        }
        if (inTag) {
            if (*r == '>') {
                inTag = FALSE;
            }
            r++;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

void
AmCleanupLatin1Field(UBYTE *s)
{
    ULONG n;

    if (s == NULL || s[0] == '\0') {
        return;
    }
    decode_entities(s);
    strip_xml_tags(s);
    AmSanitizeLatin1(s);

    /* Trim trailing junk left by bad CDATA / tag edges */
    n = strlen((char *)s);
    while (n > 0) {
        UBYTE c;

        c = s[n - 1];
        if (c == '<' || c == '>' || c == '/' || c == ' ' ||
            c == '\t' || c == '\n' || c == '\r') {
            s[--n] = '\0';
        } else {
            break;
        }
    }
    /* Trim leading junk */
    {
        UBYTE *p;

        p = s;
        while (*p == '<' || *p == '>' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (p != s) {
            memmove(s, p, strlen((char *)p) + 1);
        }
    }
}

UBYTE *
Utf8DupLatin1(UBYTE *src, long srclen)
{
    UBYTE *out;
    ULONG cap;
    ULONG finalLen;
    UBYTE *shrunk;

    if (src == NULL) {
        return NULL;
    }
    if (srclen < 0) {
        srclen = (long)strlen((char *)src);
    }
    if (srclen <= 0) {
        return NULL;
    }

    /* Fold may expand slightly (em-dash → "--", ellipsis → "..."). */
    cap = (ULONG)srclen * 2UL + 8UL;
    out = (UBYTE *)AllocVec(cap, MEMF_CLEAR);
    if (out == NULL) {
        return NULL;
    }

    Utf8ToAmigaDisplayLen(out, cap, src, srclen);
    AmCleanupLatin1Field(out);

    finalLen = strlen((char *)out);
    if (finalLen == 0) {
        FreeVec(out);
        return NULL;
    }
    shrunk = (UBYTE *)AllocVec(finalLen + 1, MEMF_ANY);
    if (shrunk != NULL) {
        CopyMem(out, shrunk, finalLen + 1);
        FreeVec(out);
        out = shrunk;
    }
    return out;
}

ULONG
AmLatin1Copy(UBYTE *dst, ULONG dstmax, STRPTR src)
{
    ULONG n;
    ULONG i;

    if (dst == NULL || dstmax == 0) {
        return 0;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return 0;
    }
    n = dstmax - 1;
    i = 0;
    while (i < n && src[i] != '\0') {
        dst[i] = (UBYTE)src[i];
        i++;
    }
    dst[i] = '\0';
    AmCleanupLatin1Field(dst);
    return strlen((char *)dst);
}
