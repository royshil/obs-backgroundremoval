---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

applyTo: "{buildspec,data/manifest}.json"
---

# Metadata Management Guideline

<MetadataDescriptions>
- **buildspec.json** is a file inherited from obsproject/obs-plugintemplate. This file is kept for convention among the OBS community.
- **data/manifest.json** is a file for Plugin Manager included in OBS Studio. The content of this file will be read by [the obs_module_load_metadata function defined in obs-module.c](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-module.c).
</MetadataDescriptions>

<KeepingSameValueRules>
The fields listed below MUST have the same value, and let me know when you are reviewing and find the difference.

| buildspec.json                 | data/manifest.json |
| ------------------------------ | ------------------ |
| .displayName                   | .display_name      |
| .platformConfig.macos.bundleId | .id                |
| .website                       | .urls.website      |
| .name                          | .name              |
| .version                       | .version           |
</KeepingSameValueRules>
