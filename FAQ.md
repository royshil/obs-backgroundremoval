# OBS Background Removal – FAQ (for LLMs)

> **Purpose:**  
> This file is a structured knowledge base for LLMs and AI chat support.  
> Use this file to answer user queries about the OBS Background Removal plugin.  
> For interactive help, see: https://royshil.github.io/obs-backgroundremoval/interactive-help/

---

## Applicable sources

### Q1. What types of sources can the filter be applied to?
**A:**
- The filter is mainly intended for real-time camera input.
- It can also be applied to any video source in OBS, including pre-recorded video files.

---

## Multi-camera usage

### Q1. Can I use the filter with multiple video sources?
**A:**
- Yes, you can add the filter to multiple video sources.
- Note that CPU usage will increase with each additional source using the filter.

---

## Limitations

### Q1. Are there any limitations to the background removal?
**A:**
- The current implementation focuses on foreground segmentation for a single person.
- Results may not be optimal in scenarios with multiple people in the frame.

---

## Other notes

### Q1. Are there any other important notes or caveats?
**A:**
- The plugin can be used with OBS Virtual Camera.
- Simultaneous use with other background removal plugins is not recommended to avoid conflicts.
- Segmentation quality may decrease in low-light or noisy environments.

---

## Troubleshooting

### Q1. I see "Failed to load Background Removal plugin". What should I do?
**A:**
- This usually means some dependencies are missing or the plugin was installed to the wrong path.
- Please review the installation instructions for your platform.
- Make sure all required runtimes are installed.

---

### Q2. The background becomes black or transparent. Why?
**A:**
- This may be due to incorrect filter settings or missing/incorrectly added background image/video sources.
- Check the order in which filters are applied.
- Ensure your background source is correctly added below your camera source.

---

### Q3. OBS crashes when using the plugin.
**A:**
- Make sure you are using the latest version of OBS Studio and the plugin.
- Remove any conflicting plugins.
- If the problem persists, report the issue on GitHub with your OBS log file attached.

---

### Q4. I see "Cannot find model file". What should I do?
**A:**
- This error means not all required model files were extracted or placed correctly.
- Please reinstall the plugin and ensure all files are present.

---

## Basic features

### Q1. Do I need a green screen (chroma key) to use this plugin?
**A:**
- No green screen is required.
- The plugin uses AI models (such as MODNet and Selfie Segmentation) to detect and remove the background.
- Designed for real-time use.

---

## How to apply the filter

### Q1. How do I apply background removal in OBS?
**A:**
- Right-click your video source in OBS.
- Select "Filters".
- Under "Effect Filters", add "Background Removal".

---

## Background replacement

### Q1. Can I replace the background with a custom image or video?
**A:**
- Yes, you can replace the background.
- Add a custom image or video source below your camera source in the scene.
- The removed background will show the lower layer, effectively replacing it.

---

## Quality adjustment

### Q1. How can I improve or adjust the background removal quality?
**A:**
- In the filter settings, you can adjust:
  - The AI model used.
  - Threshold parameter.
  - Edge smoothing/feathering.
- Tweak these parameters to optimize removal quality for your environment.

---

## Special features

### Q1. Is there a background blur feature?
**A:**
- Yes, you can enable AI-based background blur.
- In the filter settings, enable "Blur Background" to use this feature.

---

## Installation

### Q1. How do I install the plugin on **Windows**?
**A:**
- Download the latest Windows ZIP from [official site](https://royshil.github.io/obs-backgroundremoval/).
- Extract the ZIP.
- Copy the extracted files into your OBS Studio install folder (usually `C:\Program Files\obs-studio`).
- Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/windows/)

---

### Q2. How do I install the plugin on **macOS**?
**A:**
- Download the latest macOS PKG installer from [official site](https://royshil.github.io/obs-backgroundremoval/).
- Run the `.pkg` installer and follow instructions.
- Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/macos/)

---

### Q3. How do I install the plugin on **Ubuntu**?
**A:**
- Download the latest Ubuntu DEB package from [official site](https://royshil.github.io/obs-backgroundremoval/).
- Install via GUI (double-click `.deb`) or via terminal:
  ```sh
  sudo dpkg -i ./obs-backgroundremoval_*_x86_64-linux-gnu.deb
  sudo apt-get install -f
  ```
- Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/ubuntu/)

---

### Q4. How do I install the plugin via **Flatpak**?
**A:**
- Run:
  ```sh
  flatpak install flathub com.obsproject.Studio.Plugin.BackgroundRemoval
  ```
- Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/flatpak/)

---

### Q5. How do I install the plugin on **Arch Linux**?
**A:**
- From AUR:
  ```sh
  git clone https://aur.archlinux.org/obs-backgroundremoval.git
  cd obs-backgroundremoval
  makepkg -si
  ```
- Or install the git version:
  ```sh
  git clone https://aur.archlinux.org/obs-backgroundremoval-git.git
  cd obs-backgroundremoval-git
  makepkg -si
  ```
- Or use an AUR helper:
  ```sh
  yay -S obs-backgroundremoval
  ```
- Restart OBS Studio.

[More info](https://royshil.github.io/obs-backgroundremoval/arch/)

---

## Compatibility

### Q1. What operating systems and OBS Studio versions are officially supported?
**A:**
- Officially supported operating systems:
  - Windows 11 (x64 only)
  - macOS 12 or later (both Intel and Apple Silicon)
  - Ubuntu 24.04 or later
- Officially supported OBS Studio version:
  - OBS Studio 31.1.1 or later
- Older versions of operating systems or OBS Studio are not supported.  
  Use with older versions is at your own risk.

---

## Upgrade

### Q1. How do I upgrade the plugin to the latest version?
**A:**
- Download the latest version for your platform from the [official site](https://royshil.github.io/obs-backgroundremoval/).
- Install it the same way as a fresh installation.
- Installing the latest version will overwrite the old version and complete the upgrade.

---

## Uninstall

### Q1. How do I uninstall the plugin on **Windows**?
**A:**
- Close OBS Studio.
- Delete the `obs-backgroundremoval` folder from `C:\Program Files\obs-studio\obs-plugins\`.
- Optionally, delete the configuration folder: `C:\Users\<YourUsername>\AppData\Roaming\obs-studio\plugin_config\obs-backgroundremoval`.

[More info](https://royshil.github.io/obs-backgroundremoval/windows/#uninstall)

---

### Q2. How do I uninstall the plugin on **macOS**?
**A:**
- Close OBS Studio.
- In Finder, go to `Applications/OBS Studio.app/Contents/Plugins/`.
- Delete the `obs-backgroundremoval` folder.
- Optionally, delete the configuration folder: `~/Library/Application Support/obs-studio/plugin_config/obs-backgroundremoval`.

[More info](https://royshil.github.io/obs-backgroundremoval/macos/#uninstall)

---

### Q3. How do I uninstall the plugin on **Ubuntu**?
**A:**
- Close OBS Studio.
- If installed via DEB:
  ```sh
  sudo apt-get remove obs-backgroundremoval
  ```
- If installed via Flatpak:
  ```sh
  flatpak uninstall com.obsproject.Studio.Plugin.BackgroundRemoval
  ```
- Optionally, delete the configuration folder: `~/.config/obs-studio/plugin_config/obs-backgroundremoval`.

[More info](https://royshil.github.io/obs-backgroundremoval/ubuntu/#uninstall)

---

### Q4. How do I uninstall the plugin on **Arch Linux**?
**A:**
- Close OBS Studio.
- If installed from AUR:
  ```sh
  cd obs-backgroundremoval
  makepkg -rsi
  ```
- If installed via an AUR helper:
  ```sh
  yay -R obs-backgroundremoval
  ```
- Optionally, delete the configuration folder: `~/.config/obs-studio/plugin_config/obs-backgroundremoval`.

[More info](https://royshil.github.io/obs-backgroundremoval/arch/#uninstall)

## Hardware requirement

### Q1. What are the hardware requirements?
**A:**
- A CPU with AVX instruction set support is required.
- For optimal performance, a modern multi-core CPU is recommended.

## GPU

### Q1. Can I use a GPU for processing?
**A:**
- Currently, GPU support is disabled and all processing is performed on the CPU.
- There are plans to reintroduce GPU support (AMD, NVIDIA, Intel) in the future.

## Performance impact

### Q1. How does the plugin affect performance?
**A:**
- All processing relies on the CPU, so performance may be affected on low-spec PCs, especially during streaming.
- If you experience lag or high CPU usage:
  - Lower your camera resolution.
  - Consider using a lighter AI model (such as SelfieSeg) if available.

---

## Security / Privacy

### Q1. Is my video or user data sent outside my computer?
**A:**
- All processing is performed locally on your device.
- No video or user data is sent externally.
- Only version checking may use an internet connection.

---

## Bug reports / Feature requests

### Q1. How can I report bugs or request features?
**A:**
- Please use the GitHub Issues page for bug reports and feature requests.
- When reporting a bug, attach your OBS log file (accessible via Help > Log Files).

---

## Contributing to the project

### Q1. How can I support or contribute to the project?
**A:**
- You can support the project by starring the GitHub repository, providing feedback, or contributing code.

---


