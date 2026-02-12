export interface FileVerifierReleaseAsset {
  name: string;
  size: number;
  digest: string | null;
}

export interface VerifiedResultItem {
  name: string;
  sha256?: string;
  size?: number;
  error?: unknown;
}

export class FileVerifierResultDialog extends HTMLElement {
  private releaseAssets: FileVerifierReleaseAsset[] = [];
  private dialog: HTMLDialogElement;
  private listElement: HTMLDListElement;

  constructor() {
    super();
    this.attachShadow({ mode: "open" });

    this.dialog = document.createElement("dialog");

    const h2 = document.createElement("h2");
    h2.textContent = "Verification Results";
    h2.id = "dialog-title";
    this.dialog.setAttribute("aria-labelledby", "dialog-title");
    this.dialog.appendChild(h2);

    this.listElement = document.createElement("dl");
    this.dialog.appendChild(this.listElement);

    const form = document.createElement("form");
    form.method = "dialog";
    form.style.textAlign = "right";

    const button = document.createElement("button");
    button.textContent = "Close";

    form.appendChild(button);
    this.dialog.appendChild(form);

    this.shadowRoot!.appendChild(this.dialog);
  }

  connectedCallback() {
    const assetsAttr = this.getAttribute("data-release-assets");
    if (assetsAttr) {
      try {
        this.releaseAssets = JSON.parse(assetsAttr);
      } catch (e) {
        console.error("Failed to parse release assets JSON:", e);
      }
    }
  }

  public addResult(item: VerifiedResultItem) {
    const dt = document.createElement("dt");
    dt.textContent = item.name;
    dt.style.fontWeight = "bold";
    this.listElement.appendChild(dt);

    if (item.error) {
      const dd = document.createElement("dd");
      const errorMessage =
        item.error instanceof Error ? item.error.message : String(item.error);
      dd.textContent = `❌ Error: ${errorMessage}`;
      this.listElement.appendChild(dd);
      return;
    }

    if (item.sha256) {
      const expectedAsset = this.releaseAssets.find(
        (asset) => asset.digest === `sha256:${item.sha256}`,
      );

      if (expectedAsset) {
        const isSizeMatch = expectedAsset.size === item.size;

        const ddName = document.createElement("dd");
        ddName.textContent = `✅ Official Name: ${expectedAsset.name}`;
        this.listElement.appendChild(ddName);

        const ddHash = document.createElement("dd");
        ddHash.textContent = `✅ SHA-256: ${item.sha256}`;
        this.listElement.appendChild(ddHash);

        const ddSize = document.createElement("dd");
        if (isSizeMatch) {
          ddSize.textContent = `✅ Size: ${item.size} bytes`;
        } else {
          ddSize.textContent = `❌ Size: ${item.size} bytes (Expected: ${expectedAsset.size} bytes)`;
        }
        this.listElement.appendChild(ddSize);
      } else {
        const ddUnknown = document.createElement("dd");
        ddUnknown.textContent = "❌ Unknown file (SHA-256 verification failed)";
        this.listElement.appendChild(ddUnknown);
      }
    } else {
      const ddError = document.createElement("dd");
      ddError.textContent = "❌ Unknown Error (No hash provided)";
      this.listElement.appendChild(ddError);
    }
  }

  public showModal() {
    this.dialog.showModal();
  }
}
