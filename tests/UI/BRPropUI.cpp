// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <BackgroundRemoval/windows/FilterContext.hpp>
#include <BackgroundRemoval/windows/ModuleText.hpp>
#include <BackgroundRemoval/windows/AutoConfigurator.hpp>
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
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QGroupBox>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

#include <obs.hpp>
#include <util/text-lookup.h>
#include <util/platform.h>

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

auto smokeAutoConfigurator() -> bool
{
	using BackgroundRemoval::FilterContext;
	obs_source_info fixture = FilterContext::kSourceInfo;
	fixture.id = "auto-configurator-ui-fixture";
	obs_register_source(&fixture);
	OBSSourceAutoRelease source = obs_source_create_private(fixture.id, "Auto configurator UI test", nullptr);
	if (!source)
		return false;
	auto *contextHandle = static_cast<std::shared_ptr<FilterContext> *>(obs_obj_get_data(source));
	const auto context = *contextHandle;
	for (int mode = 0; mode < 3; ++mode) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_unset_user_value(settings, "v2_MotionSuppression");
		if (mode == 0)
			obs_data_set_double(settings, "v2_MotionSuppression", 12.0);
		FilterContext::handleAutoSensitivityClick(nullptr, nullptr, contextHandle);
		const auto configurator = context->autoConfigurator_;
		if (!configurator)
			return false;
		FilterContext::handleAutoSensitivityClick(nullptr, nullptr, contextHandle);
		if (context->autoConfigurator_ != configurator)
			return false;
		QEventLoop loop;
		QObject::connect(configurator, &QDialog::finished, &loop, &QEventLoop::quit);
		QTimer driver;
		QElapsedTimer elapsed;
		bool started = false;
		bool measured = false;
		bool valid = true;
		QObject::connect(&driver, &QTimer::timeout, [&] {
			if (elapsed.elapsed() > 6000) {
				valid = false;
				configurator->reject();
				return;
			}
			if (!started) {
				for (auto *button : configurator->findChildren<QPushButton *>()) {
					if (button->text() == BackgroundRemoval::tStartMotionMeasurement()) {
						button->click();
						started = true;
						break;
					}
				}
			}
			// Simulate the sole video writer without exposing private production hooks.
			context->motionMeasurement_.tick(os_gettime_ns(), 0.01);
			auto *buttons = configurator->findChild<QDialogButtonBox *>();
			if (!buttons->button(QDialogButtonBox::Apply)->isEnabled())
				return;
			measured = true;
			auto *value = configurator->findChild<QDoubleSpinBox *>();
			valid &= std::abs(value->value() - 20.0) < 0.0001;
			if (mode == 2) {
				value->setValue(17.25);
				buttons->button(QDialogButtonBox::Apply)->click();
				valid &= configurator->isVisible();
				valid &= std::abs(obs_data_get_double(settings, "v2_MotionSuppression") - 17.25) <
					 0.0001;
				value->setValue(-12.0);
				buttons->button(QDialogButtonBox::Ok)->click();
			} else {
				value->setValue(17.25);
				configurator->reject();
			}
		});
		elapsed.start();
		driver.start(20);
		loop.exec();
		driver.stop();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		if (!valid || !measured || context->autoConfigurator_)
			return false;
		if (obs_data_has_user_value(settings, "v2_MotionSuppression") != (mode != 1))
			return false;
		const double expected = mode == 0 ? 12.0 : mode == 2 ? -12.0 : 0.0;
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
				auto *configurator = new BackgroundRemoval::AutoConfigurator(nullptr);
				configurator->show();
				configurator->reject();
				delete configurator;
				if (!smokeAutoConfigurator())
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
