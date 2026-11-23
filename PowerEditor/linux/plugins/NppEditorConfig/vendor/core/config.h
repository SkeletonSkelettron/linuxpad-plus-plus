// Configuration of editorconfig-core-c for the Linux plugin.
#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define UNIX
#define HAVE_CONST
#define HAVE__BOOL
#define HAVE_STRCASECMP
#define HAVE_STRDUP
#define HAVE_STRNDUP
#define PCRE2_CODE_UNIT_WIDTH 8

#define EC_VERSION_MAJOR 0
#define EC_VERSION_MINOR 12
#define EC_VERSION_PATCH 11
#define EC_VERSION_SUFFIX ""
