// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBOBS_STUB_JANSSON_PRIVATE_CONFIG_H
#define LIBOBS_STUB_JANSSON_PRIVATE_CONFIG_H

#if defined(_WIN32)
#define HAVE_STDINT_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SETLOCALE 1
#else
#define HAVE_FCNTL_H 1
#define HAVE_SCHED_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_CLOSE 1
#define HAVE_GETPID 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_OPEN 1
#define HAVE_READ 1
#define HAVE_SCHED_YIELD 1
#define HAVE_SYNC_BUILTINS 1
#define HAVE_ATOMIC_BUILTINS 1
#define HAVE_LOCALE_H 1
#define HAVE_SETLOCALE 1
#endif
#define HAVE_INT32_T 1
#define HAVE_UINT32_T 1
#define HAVE_UINT16_T 1
#define HAVE_UINT8_T 1
#define HAVE_SSIZE_T 1
#define USE_URANDOM 1
#define INITIAL_HASHTABLE_ORDER 3

#endif
