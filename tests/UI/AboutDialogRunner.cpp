// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <UI/AboutDialog.hpp>

#include "palette.hpp"

#include <QApplication>
#include <QDebug>
#include <QString>

#include <cstdlib>

#define PLUGIN_VERSION_STR "0.0.0"

int main(int argc, char *argv[])
{
	const QString paletteName = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("Yami_dark");
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
	AboutDialog dialog(QStringLiteral(PLUGIN_VERSION_STR));
	dialog.show();
	return application.exec();
}
