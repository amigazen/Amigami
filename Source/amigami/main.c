/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * main.c - Amigami entry (ReadArgs: CAFILE/K, INSECURE/S, VERBOSE/S)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>

extern LONG AmigamiGuiRun(STRPTR cafile, BOOL insecure, BOOL verbose);
extern const char *Amigami_GetVersion(void);

#define AMIGAMI_TEMPLATE "CAFILE/K,INSECURE/S,VERBOSE/S"

struct AmigamiArgs
{
    STRPTR cafile;
    LONG insecure;
    LONG verbose;
};

int
main(void)
{
    struct RDArgs *rdargs;
    struct AmigamiArgs args;
    LONG rc;

    (void)Amigami_GetVersion();

    memset(&args, 0, sizeof(args));
    rdargs = ReadArgs(AMIGAMI_TEMPLATE, (LONG *)&args, NULL);
    if (rdargs == NULL) {
        PrintFault(IoErr(), (STRPTR)"Amigami");
        return RETURN_ERROR;
    }

    rc = AmigamiGuiRun(args.cafile,
        args.insecure ? TRUE : FALSE,
        args.verbose ? TRUE : FALSE);

    FreeArgs(rdargs);
    return (int)rc;
}
