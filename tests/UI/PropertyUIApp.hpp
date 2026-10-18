// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QApplication>
#include <QString>
#include <QWidget>

auto propertyUITranslate(const char *key) -> QString;

// The pinned OBS property widgets only need these frontend services.
class PropertyUIApp {
public:
	auto IsThemeDark() const -> bool { return QApplication::palette().color(QPalette::Window).lightness() < 128; }
	auto GetMainWindow() const -> QWidget * { return QApplication::activeWindow(); }
};

inline auto App() -> PropertyUIApp *
{
	static PropertyUIApp app;
	return &app;
}

#define QTStr(key) propertyUITranslate(key)
