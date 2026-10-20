// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <BackgroundRemoval/windows/FilterContext.hpp>
#include <BackgroundRemoval/windows/ModuleText.hpp>
#include <BackgroundRemoval/windows/AutoMotionConfigurator.hpp>
#include <BackgroundRemoval/windows/MotionMeasurement.hpp>
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
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGroupBox>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

#include <obs.hpp>
#include <util/text-lookup.h>

#include <cstdlib>
#include <cmath>
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

auto findAutoMotionDialog() -> QDialog *
{
	for (auto *widget : QApplication::topLevelWidgets())
		if (widget->objectName() == "autoMotionDialog")
			return qobject_cast<QDialog *>(widget);
	return nullptr;
}

auto smokeAutoMotionDialog() -> bool
{
	obs_source_info fixture{};
	fixture.id = "auto-motion-ui-fixture";
	fixture.type = OBS_SOURCE_TYPE_INPUT;
	fixture.get_name = [](void *) {
		return "Auto motion UI fixture";
	};
	fixture.create = [](obs_data_t *, obs_source_t *source) -> void * {
		return source;
	};
	fixture.destroy = [](void *) {
	};
	obs_register_source(&fixture);
	OBSSourceAutoRelease source = obs_source_create_private(fixture.id, "Auto motion UI test", nullptr);
	if (!source)
		return false;
	// Cancel preserves both an explicit value and an unset value; Apply keeps the result.
	for (int mode = 0; mode < 3; ++mode) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_unset_user_value(settings, "v2_MotionSuppression");
		if (mode == 0)
			obs_data_set_double(settings, "v2_MotionSuppression", 12.0);
		BackgroundRemoval::AutoMotionConfigurator configurator(source);
		QTimer driver;
		QElapsedTimer elapsed;
		bool started = false;
		bool measured = false;
		bool valid = true;
		QObject::connect(&driver, &QTimer::timeout, [&] {
			auto *dialog = findAutoMotionDialog();
			if (!dialog)
				return;
			if (elapsed.elapsed() > 6000) {
				valid = false;
				dialog->reject();
				return;
			}
			if (!started) {
				for (auto *button : dialog->findChildren<QPushButton *>()) {
					if (button->text() == BackgroundRemoval::tStartMotionMeasurement()) {
						button->click();
						started = true;
						break;
					}
				}
			}
			configurator.recordMotion(0.01);
			auto *buttons = dialog->findChild<QDialogButtonBox *>();
			if (!buttons || !buttons->button(QDialogButtonBox::Apply)->isEnabled())
				return;
			measured = true;
			if (mode == 2) {
				buttons->button(QDialogButtonBox::Apply)->click();
				valid &= dialog->isVisible();
				buttons->button(QDialogButtonBox::Ok)->click();
			} else
				dialog->reject();
		});
		elapsed.start();
		driver.start(20);
		configurator.show();
		QEventLoop loop;
		QPointer<QDialog> calibration = findAutoMotionDialog();
		if (!calibration)
			return false;
		QObject::connect(calibration, &QDialog::finished, &loop, &QEventLoop::quit);
		loop.exec();
		delete calibration.data();
		driver.stop();
		if (!valid || !measured)
			return false;
		if (obs_data_has_user_value(settings, "v2_MotionSuppression") != (mode != 1))
			return false;
		const double expected = mode == 0 ? 12.0 : mode == 2 ? 20.0 : 0.0;
		if (std::abs(obs_data_get_double(settings, "v2_MotionSuppression") - expected) > 0.0001)
			return false;
	}
	return true;
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
				BackgroundRemoval::AutoMotionConfigurator configurator(nullptr);
				configurator.recordMotion(1.0);
				const auto measurement = configurator.startMeasurement();
				if (measurement->sampleCount() != 0)
					result = EXIT_FAILURE;
				configurator.recordMotion(0.01);
				configurator.stopMeasurement();
				configurator.recordMotion(1.0);
				if (measurement->sampleCount() != 1 || !measurement->motionSuppression() ||
				    std::abs(*measurement->motionSuppression() - 20.0) > 0.0001)
					result = EXIT_FAILURE;
				configurator.show();
				if (auto *calibration = findAutoMotionDialog()) {
					calibration->reject();
					delete calibration;
				}
				if (!smokeAutoMotionDialog())
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
