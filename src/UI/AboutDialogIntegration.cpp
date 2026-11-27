// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutDialogIntegration.hpp"

#include <memory>

#include <QApplication>
#include <QDialog>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <PluginGlobal/PluginLoader.h>
#include <plugin-support.h>

#include "../UpdateChecker/UpdateChecker.hpp"
#include "AboutDialog.hpp"

namespace AboutDialogIntegration
{

namespace
{

// The context object of the wait. Deleting it cancels the wait, with every connection it has made.
QPointer<QObject> g_waiter;

void scheduleModalDialogCheck(QObject *waiter, void (*callback)() noexcept);

// Runs callback unless a modal dialog is open. Otherwise waits for that dialog to close or to be destroyed, and
// checks again, since another modal dialog may open after it.
void checkModalDialog(QObject *waiter, void (*callback)() noexcept)
{
    QWidget *const modal = QApplication::activeModalWidget();
    if (!modal)
    {
        waiter->deleteLater();
        callback();
        return;
    }

    const auto recheck = [modal, waiter, callback]() {
        QObject::disconnect(modal, nullptr, waiter, nullptr);
        scheduleModalDialogCheck(waiter, callback);
    };
    QObject::connect(modal, &QObject::destroyed, waiter, recheck);
    if (auto *const dialog = qobject_cast<QDialog *>(modal))
    {
        QObject::connect(dialog, &QDialog::finished, waiter, recheck);
    }
}

// Queued, so that the posted events before it run first. OBS posts the Auto-Configuration Wizard before
// OBS_FRONTEND_EVENT_FINISHED_LOADING, so the wizard is open when the first check runs.
void scheduleModalDialogCheck(QObject *waiter, void (*callback)() noexcept)
{
    QTimer::singleShot(0, waiter, [waiter, callback]() { checkModalDialog(waiter, callback); });
}

} // namespace

bool runWhenNoModalDialog(void (*callback)() noexcept)
{
    auto *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!parent)
    {
        obs_log_warning("Failed to wait for modal dialogs: main window is unavailable");
        return false;
    }

    cancelRunWhenNoModalDialog();

    auto *const waiter = new QObject(parent);
    g_waiter = waiter;
    scheduleModalDialogCheck(waiter, callback);
    return true;
}

void cancelRunWhenNoModalDialog() noexcept
{
    delete g_waiter.data();
}

bool show()
{
    auto *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!parent)
    {
        obs_log(LOG_WARNING, "Failed to show the About dialog: main window is unavailable");
        return false;
    }

    const bool updateNotificationsEnabled = UpdateChecker::checkForUpdatesEnabled();

    // The logo is installed with the data of the plugin. Without it, the dialog is shown without a logo.
    const std::unique_ptr<char, void (*)(void *)> logoPath(obs_module_file("logo.png"), &bfree);
    auto *dialog =
        new AboutDialog(QString::fromUtf8(PLUGIN_VERSION), logoPath ? QString::fromUtf8(logoPath.get()) : QString(),
                        parent, updateNotificationsEnabled);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog](int) {
        const bool enabled = dialog->updateNotificationsEnabled();
        if (!UpdateChecker::setCheckForUpdates(enabled))
        {
            obs_log(LOG_WARNING, "Failed to save the update notification setting to update.ini");
        }
        if (enabled && UpdateChecker::checkForUpdatesEnabled())
        {
            PluginLoaderFetchRemoteLatestVersionAsync();
        }
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return true;
}

} // namespace AboutDialogIntegration
