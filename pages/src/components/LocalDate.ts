// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

class LocalDateElement extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
  }
  connectedCallback() {
    const dateStr = new Date(this.dataset.date!).toLocaleDateString("en-CA");
    this.shadowRoot!.textContent = dateStr;
  }
}

customElements.define("local-date", LocalDateElement);
