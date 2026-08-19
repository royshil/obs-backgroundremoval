// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-FileCopyrightText: 2024 Warchamp7 <warchamp2003@hotmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace {

// Palette colors adapted from the GPL-2.0-or-later licensed OBS Studio 32.2.2 Yami theme:
// https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/data/themes/Yami.obt
enum class YamiVariant {
	Dark,
	Light,
};

inline QPalette yamiPalette(QPalette palette, YamiVariant variant)
{
	if (variant == YamiVariant::Light) {
		palette.setColor(QPalette::Window, QColor(211, 211, 211));
		palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#0A0A0A")));
		palette.setColor(QPalette::Base, QColor(229, 229, 229));
		palette.setColor(QPalette::Light, QColor(254, 254, 254));
		palette.setColor(QPalette::Mid, QColor(211, 211, 211));
		palette.setColor(QPalette::Dark, QColor(229, 229, 229));
		palette.setColor(QPalette::Highlight, QColor(140, 181, 255));
		palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#0A0A0A")));
		palette.setColor(QPalette::Text, QColor(QStringLiteral("#0A0A0A")));
		palette.setColor(QPalette::Link, QColor(QStringLiteral("#476BD7")));
		palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#476BD7")));
		palette.setColor(QPalette::Button, QColor(245, 245, 245));
		palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#0A0A0A")));
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#646464")));
		return palette;
	}

	palette.setColor(QPalette::Window, QColor(QStringLiteral("#1D1F26")));
	palette.setColor(QPalette::WindowText, Qt::white);
	palette.setColor(QPalette::Base, QColor(QStringLiteral("#272A33")));
	palette.setColor(QPalette::Light, QColor(QStringLiteral("#4E5566")));
	palette.setColor(QPalette::Mid, QColor(QStringLiteral("#1D1F26")));
	palette.setColor(QPalette::Dark, QColor(QStringLiteral("#272A33")));
	palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#284CB8")));
	palette.setColor(QPalette::HighlightedText, Qt::white);
	palette.setColor(QPalette::Text, Qt::white);
	palette.setColor(QPalette::Link, QColor(QStringLiteral("#476BD7")));
	palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#476BD7")));
	palette.setColor(QPalette::Button, QColor(QStringLiteral("#3C404D")));
	palette.setColor(QPalette::ButtonText, Qt::white);
	palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#969696")));
	return palette;
}

inline QString yamiStyleSheet(YamiVariant variant)
{
	if (variant == YamiVariant::Light) {
		return QStringLiteral(
			"QPushButton { background-color: #F5F5F5; color: #0A0A0A; border: 1px solid #F5F5F5; "
			"border-radius: 4px; padding: 2px 16px; } "
			"QPushButton:hover { background-color: #FEFEFE; border-color: #828282; } "
			"QPushButton:pressed { background-color: #D3D3D3; border-color: #F5F5F5; } "
			"QPushButton:disabled { background-color: #D3D3D3; color: #646464; }");
	}

	return QStringLiteral("QPushButton { background-color: #3C404D; color: #FFFFFF; border: 1px solid #3C404D; "
			      "border-radius: 4px; padding: 2px 16px; } "
			      "QPushButton:hover { background-color: #464B59; border-color: #5B6273; } "
			      "QPushButton:pressed { background-color: #1D1F26; border-color: #3C404D; } "
			      "QPushButton:disabled { background-color: #272A33; color: #969696; }");
}

} // namespace
