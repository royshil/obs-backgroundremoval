
# OBS Background Removal – FAQ (Knowledge Base for LLMs)

> **Purpose:**
> This file is a structured knowledge base for LLMs and AI chat support. Use it to answer user queries about the OBS Background Removal plugin. For interactive help, see: https://royshil.github.io/obs-backgroundremoval/interactive-help/

---

## 1. Basic Features & Usage

- No green screen required. AI (MODNet, Selfie Segmentation, etc.) removes the background in real time.
- Works with any video source in OBS, including cameras and video files.
- Can be applied to multiple sources (CPU usage increases with each).
- Optimized for single-person foreground segmentation. Accuracy may drop with multiple people or in low-light conditions.
- Compatible with OBS Virtual Camera. Do not use with other background removal plugins simultaneously.

### How to Apply the Filter
1. Right-click your video source in OBS → "Filters" → "Effect Filters" → Add "Background Removal".

### Customizing the Background
- Replace the background with any image or video by placing a source below your camera in the scene.
- Adjust AI model, threshold, and edge smoothing in filter settings.
- Background blur is available (enable in filter settings).

---

## 2. Installation (Detailed)

### Windows
1. Download the latest Windows ZIP from the [official site](https://royshil.github.io/obs-backgroundremoval/).
2. Extract the ZIP and copy its contents to your OBS Studio install folder (e.g., `C:\Program Files\obs-studio`).
3. Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/windows/)

---

### macOS
1. Download the latest macOS PKG installer from the [official site](https://royshil.github.io/obs-backgroundremoval/).
2. Run the PKG installer and follow the instructions.
3. Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/macos/)

---

### Ubuntu
1. Download the latest Ubuntu DEB package from the [official site](https://royshil.github.io/obs-backgroundremoval/).
2. Install via GUI (double-click `.deb`) or terminal:
   ```sh
   sudo dpkg -i ./obs-backgroundremoval_*_x86_64-linux-gnu.deb
   sudo apt-get install -f
   ```
3. Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/ubuntu/)

---

### Flatpak
1. Run:
   ```sh
   flatpak install flathub com.obsproject.Studio.Plugin.BackgroundRemoval
   ```
2. Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/flatpak/)

---

### Arch Linux
1. From AUR:
   ```sh
   git clone https://aur.archlinux.org/obs-backgroundremoval.git
   cd obs-backgroundremoval
   makepkg -si
   ```
   Or with an AUR helper:
   ```sh
   yay -S obs-backgroundremoval
   ```
2. Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/arch/)

---

## 3. Uninstallation (Summary)

- **Windows:** Close OBS Studio. Delete `obs-backgroundremoval` from `C:\Program Files\obs-studio\obs-plugins\`. Config: `AppData\Roaming\obs-studio\plugin_config\obs-backgroundremoval`.
- **macOS:** Close OBS Studio. Delete `obs-backgroundremoval` from `Applications/OBS Studio.app/Contents/Plugins/`. Config: `~/Library/Application Support/obs-studio/plugin_config/obs-backgroundremoval`.
- **Ubuntu:** `sudo apt-get remove obs-backgroundremoval` or `flatpak uninstall com.obsproject.Studio.Plugin.BackgroundRemoval`. Config: `~/.config/obs-studio/plugin_config/obs-backgroundremoval`.
- **Arch:** `yay -R obs-backgroundremoval` or similar. Config: same as Ubuntu.

---

## 4. Compatibility & Performance

- Supported OS: Windows 11 (x64), macOS 12+, Ubuntu 24.04+
- OBS Studio 31.1.1 or later required
- CPU: AVX support required. Multi-core recommended. GPU support is planned for the future.
- High CPU usage with multiple sources or high resolution. Use lighter models (e.g., SelfieSeg) if needed.

### macOS Architecture Compatibility

- **Apple Silicon (M1/M2/M3/etc.):** Use the Universal binary installer. Intel binaries running via Rosetta2 are **not supported** and will cause crashes.
- **Intel Macs:** Use the Universal binary installer. Apple Silicon binaries running on Intel are **not supported**.
- Always ensure your OBS Studio installation matches your Mac's architecture to avoid compatibility issues.

---

## 5. Troubleshooting

- **"Failed to load" error:** Missing dependencies or wrong install path. Recheck installation steps.
- **Black/transparent background:** Check filter settings and source order.
- **OBS crashes on macOS:** If running on Apple Silicon, ensure you are NOT using an Intel OBS binary via Rosetta2. If on Intel Mac, ensure you are NOT using an Apple Silicon OBS binary. The plugin does not support cross-architecture translation and will crash. Use OBS Studio and the plugin built for your Mac's native architecture.
- **OBS crashes (other platforms):** Use latest OBS/plugin, remove conflicting plugins, report with OBS log on GitHub if needed.
- **"Cannot find model file":** Missing model files. Reinstall the plugin.

---

## 6. Security & Privacy

- All processing is local. No video or user data is sent externally. Only version checking may use the internet.

---

## 7. Other

- **Bug reports/feature requests:** Use GitHub Issues. Attach OBS log for bug reports.
- **Contributing:** Star the GitHub repo, give feedback, or contribute code.

---


[More info](https://royshil.github.io/obs-backgroundremoval/flatpak/)

