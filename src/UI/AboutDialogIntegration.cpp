// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutDialogIntegration.hpp"

#include <QDialog>
#include <QString>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "AboutDialog.hpp"
#include "../UpdateConfig/UpdateConfig.hpp"

#ifndef PLUGIN_VERSION_STR
#error PLUGIN_VERSION_STR must be defined by the build system
#endif

namespace AboutDialogIntegration {
namespace {

bool showButtonClicked(obs_properties_t *, obs_property_t *, void *)
{
	static_cast<void>(show());
	return false;
}

} // namespace

bool show()
{
	auto *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (!parent) {
		blog(LOG_WARNING, OBS_LOG_HEADER "Failed to show the About dialog: main window is unavailable");
		return false;
	}

	UpdateConfig::Request getRequest = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
	const bool updateNotificationsEnabled = UpdateConfig::doRequest(getRequest);

	auto *dialog = new AboutDialog(QString::fromUtf8(PLUGIN_VERSION_STR), parent, updateNotificationsEnabled);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	QObject::connect(dialog, &QDialog::finished, dialog, [dialog](int) {
		const bool enabled = dialog->updateNotificationsEnabled();
		UpdateConfig::Request request = UpdateConfig::SetCheckForUpdatesFlagRequest{enabled};
		if (!UpdateConfig::doRequest(request)) {
			blog(LOG_WARNING,
			     OBS_LOG_HEADER "Failed to save the update notification setting to update.ini");
		}
		if (enabled) {
			UpdateConfig::Request getRequest = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
			if (UpdateConfig::doRequest(getRequest)) {
				UpdateConfig::fetchLatestVersionAsync();
			}
		}
	});
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
	return true;
}

void addButton(obs_properties_t *properties)
{
	obs_properties_add_button2(properties, "about_obs_backgroundremoval", obs_module_text("About"),
				   showButtonClicked, nullptr);
}

} // namespace AboutDialogIntegration
