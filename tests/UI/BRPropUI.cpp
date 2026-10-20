// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <BackgroundRemoval/windows/FilterContext.hpp>
#include <BackgroundRemoval/windows/ModuleText.hpp>
#include <BackgroundRemoval/windows/AutoMotionConfigurator.hpp>
#include <UI/AboutDialog.hpp>
#include <UpdateConfig/UpdateConfig.hpp>

#include "PropertyUIApp.hpp"
#include "palette.hpp"

#include <properties-view.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

#include <obs.hpp>
#include <util/text-lookup.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
lookup_t *translations = nullptr;
lookup_t *frontendTranslations = nullptr;

auto properties(void *) -> obs_properties_t *
{
	auto *result = BackgroundRemoval::FilterContext::getProperties(nullptr);
	return result;
}
} // namespace

const char *obs_module_text(const char *key)
{
	const char *text = key;
	if (translations)
		text_lookup_getstr(translations, key, &text);
	return text;
}

auto propertyUITranslate(const char *key) -> QString
{
	const char *text = key;
	if (frontendTranslations)
		text_lookup_getstr(frontendTranslations, key, &text);
	return QString::fromUtf8(text);
}

namespace UpdateConfig {
std::optional<std::string> getLatestVersion()
{
	return std::nullopt;
}
} // namespace UpdateConfig

namespace AboutDialogIntegration {
bool show()
{
	auto *dialog = new AboutDialog(QStringLiteral(PLUGIN_VERSION_STR), QApplication::activeWindow(), false);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
	return true;
}
} // namespace AboutDialogIntegration

int main(int argc, char **argv)
{
	const QString firstArgument = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("Yami_dark");
	const QString paletteName = firstArgument == QStringLiteral("--smoke-test") ? QStringLiteral("Yami_dark")
										    : firstArgument;
	YamiVariant variant;
	if (paletteName == QStringLiteral("Yami_dark")) {
		variant = YamiVariant::Dark;
	} else if (paletteName == QStringLiteral("Yami_light")) {
		variant = YamiVariant::Light;
	} else {
		qCritical().noquote() << "Unknown palette:" << paletteName << "\nExpected Yami_dark or Yami_light.";
		return EXIT_FAILURE;
	}
	QApplication application(argc, argv);
	application.setPalette(yamiPalette(application.palette(), variant));
	application.setStyleSheet(yamiStyleSheet(variant));
	if (!obs_startup("en-US", nullptr, nullptr))
		return EXIT_FAILURE;
	struct Cleanup {
		~Cleanup()
		{
			text_lookup_destroy(translations);
			text_lookup_destroy(frontendTranslations);
			translations = frontendTranslations = nullptr;
			obs_shutdown();
		}
	} cleanup;
	try {
		translations = text_lookup_create(FILTER_PROPERTY_LOCALE);
		frontendTranslations = text_lookup_create(OBS_FRONTEND_LOCALE);
		if (!translations || !frontendTranslations)
			throw std::runtime_error("Cannot load property translations");
		OBSDataAutoRelease settings = obs_data_create();
		BackgroundRemoval::FilterContext::getDefaults(settings);
		QDialog dialog;
		dialog.setWindowTitle("Properties for Background Removal");
		auto *layout = new QVBoxLayout(&dialog);
		auto *view = new OBSPropertiesView(settings.Get(), "background_removal", properties);
		layout->addWidget(view);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
						     QDialogButtonBox::RestoreDefaults);
		QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		auto *data = settings.Get();
		QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, view,
				 [view, data] {
					 obs_data_clear(data);
					 BackgroundRemoval::FilterContext::getDefaults(data);
					 view->ReloadProperties();
				 });
		layout->addWidget(buttons);
		dialog.resize(720, 640);
		dialog.show();
		int result = EXIT_SUCCESS;
		if (application.arguments().contains("--smoke-test")) {
			QTimer::singleShot(500, &dialog, [&] {
				if (view->findChildren<QGroupBox *>().isEmpty())
					result = EXIT_FAILURE;
				const auto combos = view->findChildren<QComboBox *>();
				if (combos.size() != 1 || combos.front()->currentData().toLongLong() != 0 ||
				    combos.front()->currentText() != BackgroundRemoval::tPipelineAuto())
					result = EXIT_FAILURE;
				OBSDataAutoRelease selection = obs_data_create();
				BackgroundRemoval::FilterContext::getDefaults(selection);
				if (obs_data_get_int(selection, "v2_PipelineId") != 0)
					result = EXIT_FAILURE;
				obs_data_set_int(selection, "v2_PipelineId", 62828);
				if (obs_data_get_int(selection, "v2_PipelineId") != 62828)
					result = EXIT_FAILURE;
				obs_data_set_int(selection, "v2_PipelineId", 0);
				if (obs_data_get_int(selection, "v2_PipelineId") != 0)
					result = EXIT_FAILURE;
				BackgroundRemoval::AutoMotionConfigurator calibration(nullptr, nullptr, &dialog);
				QTimer::singleShot(0, &calibration, &QDialog::reject);
				if (calibration.exec() != QDialog::Rejected)
					result = EXIT_FAILURE;
				const auto arguments = application.arguments();
				const int screenshot = arguments.indexOf("--screenshot");
				if (screenshot >= 0 && (screenshot + 1 >= arguments.size() ||
							!dialog.grab().save(arguments[screenshot + 1])))
					result = EXIT_FAILURE;
				dialog.close();
			});
		}
		application.exec();
		return result;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
