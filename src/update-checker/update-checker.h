/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void check_update(void);
const char *get_latest_version(void);

#ifdef __cplusplus
}
#endif
