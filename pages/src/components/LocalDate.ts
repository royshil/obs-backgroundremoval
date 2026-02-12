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
