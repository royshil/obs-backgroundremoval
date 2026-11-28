// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc23-extensions"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

#if defined(__has_embed)
#define has_embed(x) __has_embed(x)
#else
#define has_embed(x) 0
#endif

extern "C" const char kLegalLicense[] =
#if has_embed("../LICENSE")
    {
#embed "../LICENSE"
        , 0}
#elif __has_include(<LICENSE.inc>)
#include <LICENSE.inc>
#else
#error "No LICENSE"
#endif
;

extern "C" const char kLegalNotice[] =
#if has_embed("../NOTICE")
    {
#embed "../NOTICE"
        , 0}
#elif __has_include(<NOTICE.inc>)
#include <NOTICE.inc>
#else
#error "No NOTICE"
#endif
;

extern "C" const char kLegalLicensesApache_20[] =
#if has_embed("../LICENSES/Apache-2.0.txt")
    {
#embed "../LICENSES/Apache-2.0.txt"
        , 0}
#elif __has_include(<LICENSES/Apache-2.0.txt.inc>)
#include <LICENSES/Apache-2.0.txt.inc>
#else
#error "No LICENSES/Apache-2.0.txt"
#endif
;

extern "C" const char kLegalLicensesCcBy_40[] =
#if has_embed("../LICENSES/CC-BY-4.0.txt")
    {
#embed "../LICENSES/CC-BY-4.0.txt"
        , 0}
#elif __has_include(<LICENSES/CC-BY-4.0.txt.inc>)
#include <LICENSES/CC-BY-4.0.txt.inc>
#else
#error "No LICENSES/CC-BY-4.0.txt"
#endif
;

extern "C" const char kLegalLicensesCcBySa_40[] =
#if has_embed("../LICENSES/CC-BY-SA-4.0.txt")
    {
#embed "../LICENSES/CC-BY-SA-4.0.txt"
        , 0}
#elif __has_include(<LICENSES/CC-BY-SA-4.0.txt.inc>)
#include <LICENSES/CC-BY-SA-4.0.txt.inc>
#else
#error "No LICENSES/CC-BY-SA-4.0.txt"
#endif
;

extern "C" const char kLegalLicensesCc0_10[] =
#if has_embed("../LICENSES/CC0-1.0.txt")
    {
#embed "../LICENSES/CC0-1.0.txt"
        , 0}
#elif __has_include(<LICENSES/CC0-1.0.txt.inc>)
#include <LICENSES/CC0-1.0.txt.inc>
#else
#error "No LICENSES/CC0-1.0.txt"
#endif
;

extern "C" const char kLegalLicensesCurl[] =
#if has_embed("../LICENSES/curl.txt")
    {
#embed "../LICENSES/curl.txt"
        , 0}
#elif __has_include(<LICENSES/curl.txt.inc>)
#include <LICENSES/curl.txt.inc>
#else
#error "No LICENSES/curl.txt"
#endif
;

extern "C" const char kLegalLicensesGpl2OrLater[] =
#if has_embed("../LICENSES/GPL-2.0-or-later.txt")
    {
#embed "../LICENSES/GPL-2.0-or-later.txt"
        , 0}
#elif __has_include(<LICENSES/GPL-2.0-or-later.txt.inc>)
#include <LICENSES/GPL-2.0-or-later.txt.inc>
#else
#error "No LICENSES/GPL-2.0-or-later.txt"
#endif
;

extern "C" const char kLegalLicensesGpl3OrLater[] =
#if has_embed("../LICENSES/GPL-3.0-or-later.txt")
    {
#embed "../LICENSES/GPL-3.0-or-later.txt"
        , 0}
#elif __has_include(<LICENSES/GPL-3.0-or-later.txt.inc>)
#include <LICENSES/GPL-3.0-or-later.txt.inc>
#else
#error "No LICENSES/GPL-3.0-or-later.txt"
#endif
;

extern "C" const char kLegalLicensesMit[] =
#if has_embed("../LICENSES/MIT.txt")
    {
#embed "../LICENSES/MIT.txt"
        , 0}
#elif __has_include(<LICENSES/MIT.txt.inc>)
#include <LICENSES/MIT.txt.inc>
#else
#error "No LICENSES/MIT.txt"
#endif
;

extern "C" const char *const kLegalAll[] = {
    "# Open Source Licenses\n\n## LICENSE (obs-backgroundremoval)\n\n",
    kLegalLicense,
    "\n\n## NOTICE (obs-backgroundremoval)\n\n",
    kLegalNotice,
    "\n\n## Apache-2.0\n\n",
    kLegalLicensesApache_20,
    "\n\n## CC-BY-4.0\n\n",
    kLegalLicensesCcBy_40,
    "\n\n## CC-BY-SA-4.0\n\n",
    kLegalLicensesCcBySa_40,
    "\n\n## CC0-1.0\n\n",
    kLegalLicensesCc0_10,
    "\n\n## curl\n\n",
    kLegalLicensesCurl,
    "\n\n## GPL-2.0-or-later\n\n",
    kLegalLicensesGpl2OrLater,
    "\n\n## GPL-3.0-or-later\n\n",
    kLegalLicensesGpl3OrLater,
    "\n\n## MIT\n\n",
    kLegalLicensesMit,
    0,
};
