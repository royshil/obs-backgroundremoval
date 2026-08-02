// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import { LATEST_RELEASE } from "../../lib/releases.js";

export async function GET() {
  return new Response(LATEST_RELEASE.version, {
    status: 200,
    headers: {
      "Content-Type": "text/plain",
    },
  });
}
