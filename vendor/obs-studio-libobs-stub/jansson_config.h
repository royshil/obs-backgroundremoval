// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBOBS_STUB_JANSSON_CONFIG_H
#define LIBOBS_STUB_JANSSON_CONFIG_H

#define JANSSON_USING_CMAKE
#define JSON_INLINE inline
#define json_int_t long long
#define json_strtoint strtoll
#if defined(_WIN32)
#define JSON_INTEGER_FORMAT "I64d"
#else
#define JSON_INTEGER_FORMAT "lld"
#endif
#define JSON_HAVE_LOCALECONV 1
#define HAVE_STDINT_H 1
#define JSON_PARSER_MAX_DEPTH 2048

#endif
