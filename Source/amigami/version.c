/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 */

#include <exec/types.h>

static const char version_tag[] = "\0$VER: Amigami 0.1 (25.7.2026)";
static const char stack_cookie[] = "$STACK: 32768";

const char *
Amigami_GetVersion(void)
{
    (void)stack_cookie;
    return version_tag + 1;
}
