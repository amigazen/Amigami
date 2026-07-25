/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * fetch.c - amihttp Tier 2 feed download
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/amihttp.h>
#include <string.h>
#include <stdio.h>

#include "fetch.h"

extern struct Library *HttpBase;

static void
set_err(STRPTR errBuf, ULONG errMax, STRPTR msg)
{
    if (errBuf == NULL || errMax == 0) {
        return;
    }
    if (msg == NULL) {
        msg = (STRPTR)"error";
    }
    strncpy((char *)errBuf, (char *)msg, errMax - 1);
    errBuf[errMax - 1] = '\0';
}

BOOL
AmFetchInit(struct AmFetchSession *fs, STRPTR cafile, BOOL insecure,
    BOOL verbose)
{
    if (fs == NULL) {
        return FALSE;
    }
    memset(fs, 0, sizeof(*fs));
    fs->afs_Insecure = insecure;
    fs->afs_Verbose = verbose;
    if (cafile != NULL && cafile[0] != '\0') {
        strncpy((char *)fs->afs_CaFile, (char *)cafile,
            sizeof(fs->afs_CaFile) - 1);
    } else {
        strcpy((char *)fs->afs_CaFile, "DEVS:Certificates/cacert.pem");
    }

    if (HttpBase == NULL) {
        return FALSE;
    }

    fs->afs_Session = NewHttpSession();
    if (fs->afs_Session == NULL) {
        return FALSE;
    }

    SetHttpSessionAttrs(fs->afs_Session,
        HTSA_USERAGENT, (ULONG)"Amigami/0.1 (amihttp)",
        HTSA_CA_BUNDLE_PATH, (ULONG)fs->afs_CaFile,
        HTSA_SSL_VERIFY, insecure ? (ULONG)HTSSL_VERIFY_NONE
            : (ULONG)HTSSL_VERIFY_PEER,
        TAG_DONE);

    return TRUE;
}

void
AmFetchShutdown(struct AmFetchSession *fs)
{
    if (fs == NULL) {
        return;
    }
    if (fs->afs_Session != NULL) {
        DisposeHttpSession(fs->afs_Session);
        fs->afs_Session = NULL;
    }
}

static BOOL
read_all_body(struct HttpTransaction *txn, UBYTE **outBody, ULONG *outLen)
{
    UBYTE chunk[4096];
    UBYTE *buf;
    ULONG cap;
    ULONG used;
    LONG n;

    *outBody = NULL;
    if (outLen != NULL) {
        *outLen = 0;
    }
    buf = NULL;
    cap = 0;
    used = 0;

    for (;;) {
        n = HttpTransactionReadBody(txn, chunk, sizeof(chunk));
        if (n < 0) {
            if (buf != NULL) {
                FreeVec(buf);
            }
            return FALSE;
        }
        if (n == 0) {
            break;
        }
        if (used + (ULONG)n + 1 > AMFETCH_MAX_BODY) {
            if (buf != NULL) {
                FreeVec(buf);
            }
            return FALSE;
        }
        if (used + (ULONG)n + 1 > cap) {
            ULONG ncap;
            UBYTE *nbuf;

            ncap = (cap == 0) ? 8192UL : (cap * 2UL);
            while (ncap < used + (ULONG)n + 1) {
                ncap *= 2UL;
            }
            if (ncap > AMFETCH_MAX_BODY) {
                ncap = AMFETCH_MAX_BODY;
            }
            nbuf = (UBYTE *)AllocVec(ncap, MEMF_ANY);
            if (nbuf == NULL) {
                if (buf != NULL) {
                    FreeVec(buf);
                }
                return FALSE;
            }
            if (buf != NULL && used > 0) {
                CopyMem(buf, nbuf, used);
                FreeVec(buf);
            }
            buf = nbuf;
            cap = ncap;
        }
        CopyMem(chunk, buf + used, (ULONG)n);
        used += (ULONG)n;
        buf[used] = '\0';
    }

    if (buf == NULL) {
        buf = (UBYTE *)AllocVec(1, MEMF_CLEAR);
        if (buf == NULL) {
            return FALSE;
        }
        used = 0;
    }

    *outBody = buf;
    if (outLen != NULL) {
        *outLen = used;
    }
    return TRUE;
}

BOOL
AmFetchUrl(struct AmFetchSession *fs, STRPTR url,
    UBYTE **outBody, ULONG *outLen, LONG *outHttpStatus, STRPTR errBuf,
    ULONG errMax)
{
    struct HttpTransaction *txn;
    LONG status;
    BOOL ok;

    if (outBody != NULL) {
        *outBody = NULL;
    }
    if (outLen != NULL) {
        *outLen = 0;
    }
    if (outHttpStatus != NULL) {
        *outHttpStatus = 0;
    }

    if (fs == NULL || fs->afs_Session == NULL || url == NULL ||
        outBody == NULL) {
        set_err(errBuf, errMax, (STRPTR)"bad fetch args");
        return FALSE;
    }

    txn = NewHttpTransaction(fs->afs_Session);
    if (txn == NULL) {
        set_err(errBuf, errMax, (STRPTR)"NewHttpTransaction failed");
        return FALSE;
    }

    ok = SetHttpTransactionAttrs(txn,
        HTTA_URL, (ULONG)url,
        HTTA_METHOD, (ULONG)"GET",
        TAG_DONE);
    if (!ok) {
        DisposeHttpTransaction(txn);
        set_err(errBuf, errMax, (STRPTR)"SetHttpTransactionAttrs failed");
        return FALSE;
    }

    HttpTransactionAddHeader(txn, (STRPTR)"Accept",
        (STRPTR)"application/rss+xml, application/atom+xml, "
        "application/xml, text/xml, */*");

    if (!HttpTransactionPerform(txn)) {
        LONG herr;

        herr = HttpTransactionGetLastError(txn);
        DisposeHttpTransaction(txn);
        if (errBuf != NULL && errMax > 0) {
            sprintf((char *)errBuf, "HTTP perform failed (%ld)", herr);
        }
        return FALSE;
    }

    status = HttpTransactionGetStatusCode(txn);
    if (outHttpStatus != NULL) {
        *outHttpStatus = status;
    }

    if (!read_all_body(txn, outBody, outLen)) {
        DisposeHttpTransaction(txn);
        set_err(errBuf, errMax, (STRPTR)"ReadBody failed");
        return FALSE;
    }

    DisposeHttpTransaction(txn);

    if (status < 200 || status >= 300) {
        if (*outBody != NULL) {
            FreeVec(*outBody);
            *outBody = NULL;
        }
        if (outLen != NULL) {
            *outLen = 0;
        }
        if (errBuf != NULL && errMax > 0) {
            sprintf((char *)errBuf, "HTTP status %ld", status);
        }
        return FALSE;
    }

    return TRUE;
}
