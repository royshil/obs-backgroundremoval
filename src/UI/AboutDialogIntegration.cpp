// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutDialogIntegration.hpp"

#include <memory>

#include <QDialog>
#include <QString>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <plugin-support.h>

#include "../UpdateConfig/UpdateConfig.hpp"
#include "AboutDialog.hpp"

namespace AboutDialogIntegration
{

bool show()
{
    auto *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!parent)
    {
        obs_log(LOG_WARNING, "Failed to show the About dialog: main window is unavailable");
        return false;
    }

    UpdateConfig::Request getRequest = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
    const bool updateNotificationsEnabled = UpdateConfig::doRequest(getRequest);

    // The logo is installed with the data of the plugin. Without it, the dialog is shown without a logo.
    const std::unique_ptr<char, void (*)(void *)> logoPath(obs_module_file("logo.png"), &bfree);
    auto *dialog =
        new AboutDialog(QString::fromUtf8(PLUGIN_VERSION), logoPath ? QString::fromUtf8(logoPath.get()) : QString(),
                        parent, updateNotificationsEnabled);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog](int) {
        const bool enabled = dialog->updateNotificationsEnabled();
        UpdateConfig::Request request = UpdateConfig::SetCheckForUpdatesFlagRequest{enabled};
        if (!UpdateConfig::doRequest(request))
        {
            obs_log(LOG_WARNING, "Failed to save the update notification setting to update.ini");
        }
        if (enabled)
        {
            UpdateConfig::Request getRequest = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
            if (UpdateConfig::doRequest(getRequest))
            {
                UpdateConfig::fetchLatestVersionAsync();
            }
        }
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return true;
}

} // namespace AboutDialogIntegration
