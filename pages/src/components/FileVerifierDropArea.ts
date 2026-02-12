class FileVerifierDropAreaElement extends HTMLElement {
  l!: HTMLLabelElement;
  c?: AbortController;

  static sheet = new CSSStyleSheet();

  static {
    this.sheet.replaceSync(`
        :host { display: block; }
        label {
          height: 7rem;
          display: flex;
          align-items: center;
          justify-content: center;
          flex-direction: column;
          border: 2px dashed #ccc;
          border-radius: 20px;
          transition: transform .05s;
          & * { pointer-events: none; }
        }
        .active {
          border-color: #2196F3;
          transform: scale(1.02);
        }
      `);
  }

  constructor() {
    super();
    this.attachShadow({ mode: "open" });

    const shadowRoot = this.shadowRoot!;

    shadowRoot.adoptedStyleSheets = [FileVerifierDropAreaElement.sheet];

    const label = document.createElement("label");

    const div1 = document.createElement("div");
    div1.textContent = "Drag & Drop file(s) here to verify";

    const div2 = document.createElement("div");
    div2.textContent = "No data will be sent to the Internet";

    label.append(div1, div2);

    shadowRoot.append(label);

    this.l = label;
  }

  connectedCallback() {
    const label = this.l;
    this.c = new AbortController();
    const { signal } = this.c;
    label.addEventListener(
      "dragover",
      (e) => {
        e.preventDefault();
        e.stopPropagation();
        label.classList.add("active");
      },
      { signal },
    );
    label.addEventListener(
      "dragleave",
      (e) => {
        e.preventDefault();
        e.stopPropagation();
        label.classList.remove("active");
      },
      { signal },
    );
    label.addEventListener(
      "drop",
      async (e: DragEvent) => {
        e.preventDefault();
        e.stopPropagation();
        label.classList.remove("active");
        if (!e.dataTransfer) return;
        for (const file of e.dataTransfer.files) {
          try {
            const buffer = await file.arrayBuffer();
            const sha256Buffer = await crypto.subtle.digest("SHA-256", buffer);
            this.dispatchEvent(
              new CustomEvent("file-verifier-file-dropped", {
                detail: {
                  name: file.name,
                  size: file.size,
                  sha256Buffer: sha256Buffer,
                },
                bubbles: true,
                composed: true,
              }),
            );
          } catch (error) {
            this.dispatchEvent(
              new CustomEvent("file-verifier-error", {
                detail: {
                  name: file.name,
                  size: file.size,
                  error,
                },
                bubbles: true,
                composed: true,
              }),
            );
          }
        }
      },
      { signal },
    );
  }

  disconnectedCallback() {
    this.c?.abort();
  }
}

customElements.define("file-verifier-drop-area", FileVerifierDropAreaElement);
