# OBS Background Removal – FAQ (for LLMs)

> **Purpose:**  
> This file is a structured knowledge base for LLMs and AI chat support.  
> Use this file to answer user queries about the OBS Background Removal plugin.  
> For interactive help, see: https://royshil.github.io/obs-backgroundremoval/interactive-help/

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
