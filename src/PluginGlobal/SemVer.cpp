/*
 * SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SemVer.hpp"

bool PluginGlobal::SemVer::parse(const std::string_view str) noexcept
{
    std::size_t i = 0;

    char phrase = '0';
    std::size_t ph_start = 0;
    std::size_t ph_pos = 0;

    std::size_t j = 0;

    preReleaseLen = 0;
    buildLen = 0;

    for (;;)
    {
        switch (phrase)
        {
        case '\0': /* acceptable end */
            if (i == str.size())
            {
                return true;
            }
            else
            {
                phrase = 'u'; /* unexpected char */
                continue;
            }

        case '0': /* <version core> */
            ph_start = i;

            if (i < str.size() && str[i] == '0')
            {
                version[ph_pos] = 0;
                i += 1;
            }
            else if (i < str.size() && str[i] >= '1' && str[i] <= '9')
            {
                version[ph_pos] = str[i] - '0';
                i += 1;
                while (i < str.size() && str[i] >= '0' && str[i] <= '9' && i - ph_start < 18)
                {
                    version[ph_pos] = version[ph_pos] * 10 + (str[i] - '0');
                    i += 1;
                }

                if (i < str.size() && str[i] >= '0' && str[i] <= '9')
                {
                    phrase = 'o'; /* too large number */
                    continue;
                }
            }
            else
            {
                phrase = 'u'; /* unexpected char */
                continue;
            }

            if (ph_pos < 2)
            {
                if (i < str.size() && str[i] == '.')
                {
                    i += 1;
                    phrase = '0'; /* <version core> */
                    ph_pos += 1;  /* next version core component */
                    continue;
                }
                else
                {
                    phrase = 'u'; /* unexpected char */
                    continue;
                }
            }

            if (i < str.size() && (str[i] == '-' || str[i] == '+'))
            {
                phrase = str[i]; /* <pre-release> or <build> */
                i += 1;
                ph_start = i;
                ph_pos = i;
                continue;
            }
            else
            {
                phrase = '\0'; /* acceptable end */
                continue;
            }
        case '-': /* <pre-release> */
        case '+': /* <build> */
        {
            while (i < str.size() && str[i] >= '0' && str[i] <= '9')
            {
                i += 1;
            }

            j = i; /* end of the leading digits */

            while (i < str.size() && ((str[i] == '-') || (str[i] >= '0' && str[i] <= '9') ||
                                      (str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z')))
            {
                i += 1;
            }

            if (i == ph_pos)
            {
                phrase = 'u'; /* empty identifier */
                continue;
            }
            else if (phrase == '-' && j == i && i - ph_pos > 1 && str[ph_pos] == '0')
            {
                phrase = 'z'; /* leading zero in numeric identifier */
                continue;
            }
            else if (i < str.size() && str[i] == '.')
            {
                i += 1;
                ph_pos = i; /* next identifier */
                continue;
            }
            else
            {
                phrase |= 0x40; /* <pre-release> copy: m, <build> copy: k */
                continue;
            }
        }

        case 'm': /* <pre-release> copy: data[ph_start] to data[i - 1] */
            if (i - ph_start > preReleaseBuf.size())
            {
                phrase = 'f'; /* buffer full */
                continue;
            }

            for (j = ph_start; j < i; j++)
            {
                preReleaseBuf[j - ph_start] = str[j];
            }
            preReleaseLen = i - ph_start;

            if (i < str.size() && str[i] == '+')
            {
                phrase = '+'; /* <build> */
                i += 1;
                ph_start = i;
                ph_pos = i;
                continue;
            }
            else
            {
                phrase = '\0'; /* acceptable end */
                continue;
            }
        case 'k': /* <build> copy: data[ph_start] to data[i - 1] */
            if (i - ph_start > buildBuf.size())
            {
                phrase = 'f'; /* buffer full */
                continue;
            }

            for (j = ph_start; j < i; j++)
            {
                buildBuf[j - ph_start] = str[j];
            }
            buildLen = i - ph_start;

            phrase = '\0'; /* acceptable end */
            continue;
        default: /* rejected */
            version[0] = 0;
            version[1] = 0;
            version[2] = 0;

            preReleaseLen = 0;
            buildLen = 0;

            break;
        }

        break;
    }

    return false;
}
