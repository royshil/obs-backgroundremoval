// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AboutDialog.hpp"

#if __has_include(<legal.hpp>)
#include <legal.hpp>
#define ABOUT_DIALOG_HAS_LEGAL_TEXT
#endif

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(const QString &version, QWidget *parent, bool updateNotificationsEnabled)
	: QDialog(parent),
	  logoLabel(new QLabel(this)),
	  productNameLabel(new QLabel(this)),
	  authorLabel(new QLabel(this)),
	  versionLabel(new QLabel(this)),
	  officialSiteLink(new QLabel(this)),
	  officialSiteSeparator(new QLabel(this)),
	  githubLink(new QLabel(this)),
	  githubSeparator(new QLabel(this)),
	  communityLink(new QLabel(this)),
	  communitySeparator(new QLabel(this)),
	  obsForumLink(new QLabel(this)),
	  headerSeparator(new QFrame(this)),
	  descriptionLabel(new QLabel(this)),
	  copyrightLabel(new QLabel(this)),
	  legalNoticeLabel(new QLabel(this)),
	  updateNotificationFrame(new QFrame(this)),
	  enableUpdateNotifications(new QCheckBox(updateNotificationFrame)),
	  updateCheckDescription(new QLabel(updateNotificationFrame)),
	  updateCheckUrlLabel(new QLabel(updateNotificationFrame)),
	  buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok, this)),
	  licensesButton(new QPushButton(this)),
	  aboutQtButton(new QPushButton(this)),
	  licensesDialog(new QDialog(this)),
	  licensesTextEdit(new QPlainTextEdit(licensesDialog)),
	  licensesButtonBox(new QDialogButtonBox(QDialogButtonBox::Close, licensesDialog))
{
	setupUi(version, updateNotificationsEnabled);
}

AboutDialog::~AboutDialog() noexcept = default;

bool AboutDialog::updateNotificationsEnabled() const
{
	return enableUpdateNotifications->isChecked();
}

void AboutDialog::setupUi(const QString &version, bool updateNotificationsEnabled)
{
	setWindowTitle(tr("About obs-backgroundremoval"));
	setModal(true);

	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setSizeConstraint(QLayout::SetFixedSize);
	rootLayout->addStrut(640);
	auto *headerLayout = new QHBoxLayout;

	QPixmap logo(QStringLiteral(":/obs-backgroundremoval-resources/logo.png"));
	if (!logo.isNull()) {
		logoLabel->setPixmap(logo.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
	logoLabel->setAlignment(Qt::AlignCenter);
	headerLayout->addWidget(logoLabel);

	auto *productLayout = new QVBoxLayout;
	productNameLabel->setText(tr("Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement"));
	QFont productNameFont = productNameLabel->font();
	productNameFont.setPointSizeF(productNameFont.pointSizeF() * 1.5);
	productNameFont.setBold(true);
	productNameLabel->setFont(productNameFont);
	productNameLabel->setWordWrap(true);
	productLayout->addWidget(productNameLabel);

	const QString royshilLink =
		QStringLiteral("<a href=\"https://github.com/royshil\">%1</a>").arg(tr("Roy Shilkrot (royshil)"));
	const QString umireonLink =
		QStringLiteral("<a href=\"https://github.com/umireon\">%1</a>").arg(tr("Kaito Udagawa (umireon)"));
	authorLabel->setText(tr("%1 and %2").arg(royshilLink, umireonLink));
	productLayout->addWidget(authorLabel);
	const QString latestVersionLink =
		QStringLiteral("<a href=\"https://github.com/royshil/obs-backgroundremoval/releases\">%1</a>")
			.arg(tr("Check for latest version"));
	versionLabel->setText(tr("Version %1 — %2").arg(version, latestVersionLink));
	productLayout->addWidget(versionLabel);
	headerLayout->addLayout(productLayout, 1);
	rootLayout->addLayout(headerLayout);

	headerSeparator->setFrameShape(QFrame::HLine);
	headerSeparator->setFrameShadow(QFrame::Sunken);
	rootLayout->addWidget(headerSeparator);

	descriptionLabel->setText(
		tr("An OBS plugin for removing background in portrait images (video), making it easy to "
		   "replace the background when recording or streaming."));
	descriptionLabel->setWordWrap(true);
	rootLayout->addWidget(descriptionLabel);

	auto *linksLayout = new QHBoxLayout;
	officialSiteLink->setText(QStringLiteral("<a href=\"https://royshil.github.io/obs-backgroundremoval/\">%1</a>")
					  .arg(tr("Official Site")));
	linksLayout->addWidget(officialSiteLink);
	linksLayout->addWidget(officialSiteSeparator);
	githubLink->setText(
		QStringLiteral("<a href=\"https://github.com/royshil/obs-backgroundremoval\">%1</a>").arg(tr("GitHub")));
	linksLayout->addWidget(githubLink);
	linksLayout->addWidget(githubSeparator);
	communityLink->setText(
		QStringLiteral("<a href=\"https://github.com/royshil/obs-backgroundremoval/discussions\">%1</a>")
			.arg(tr("Community")));
	linksLayout->addWidget(communityLink);
	linksLayout->addWidget(communitySeparator);
	obsForumLink->setText(QStringLiteral("<a href=\"https://obsproject.com/forum/resources/"
					     "background-removal-virtual-green-screen-low-light-enhance.1260/\">%1</a>")
				      .arg(tr("OBS Forum")));
	linksLayout->addWidget(obsForumLink);
	linksLayout->addStretch();
	rootLayout->addLayout(linksLayout);

	updateNotificationFrame->setAutoFillBackground(true);
	updateNotificationFrame->setBackgroundRole(QPalette::Base);
	auto *updateNotificationLayout = new QVBoxLayout(updateNotificationFrame);

	enableUpdateNotifications->setText(tr("Notify me about new versions on its Filter Properties screen"));
	QFont updateNotificationHeadingFont = enableUpdateNotifications->font();
	updateNotificationHeadingFont.setBold(true);
	enableUpdateNotifications->setFont(updateNotificationHeadingFont);
	enableUpdateNotifications->setChecked(updateNotificationsEnabled);
	updateNotificationLayout->addWidget(enableUpdateNotifications);

	const QString updateCheckDescriptionText =
		tr("When OBS Studio starts, the plugin downloads one plain-text file from our project's GitHub Pages "
		   "site [1]. It sends no additional information. This copy of the plugin never sends diagnostic, "
		   "analytics, or usage data to its authors or to any third party other than GitHub. Whenever a new "
		   "version is available, a small message appears in the first row of the filter settings. The plugin "
		   "never downloads or installs updates.");
	updateCheckDescription->setText(QStringLiteral("<div style=\"line-height: 80%;\">%1</div>")
						.arg(updateCheckDescriptionText.toHtmlEscaped()));
	updateCheckDescription->setTextFormat(Qt::RichText);
	updateCheckDescription->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	updateCheckDescription->setWordWrap(true);
	updateCheckDescription->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	updateCheckDescription->setContentsMargins(24, 0, 0, 0);
	updateNotificationLayout->addWidget(updateCheckDescription);

	updateCheckUrlLabel->setText(
		QStringLiteral("[1] <a href=\"https://royshil.github.io/obs-backgroundremoval/metadata/"
			       "latest-version.txt\">https://royshil.github.io/obs-backgroundremoval/metadata/"
			       "latest-version.txt</a>"));
	updateCheckUrlLabel->setTextFormat(Qt::RichText);
	updateCheckUrlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::TextSelectableByKeyboard);
	updateCheckUrlLabel->setOpenExternalLinks(true);
	updateCheckUrlLabel->setWordWrap(true);
	updateCheckUrlLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	updateCheckUrlLabel->setContentsMargins(24, 0, 0, 0);
	updateNotificationLayout->addWidget(updateCheckUrlLabel);
	rootLayout->addWidget(updateNotificationFrame);

	copyrightLabel->setText(tr("Copyright © 2021–2026 Roy Shilkrot and © 2023–2026 Kaito Udagawa."));
	copyrightLabel->setWordWrap(true);
	rootLayout->addWidget(copyrightLabel);

	legalNoticeLabel->setText(
		tr("Licensed under the GNU General Public License v3.0 or later. This plugin includes "
		   "third-party software and pretrained models provided under their respective licenses."));
	legalNoticeLabel->setWordWrap(true);
	rootLayout->addWidget(legalNoticeLabel);

	for (QLabel *label : {productNameLabel, descriptionLabel, copyrightLabel, legalNoticeLabel}) {
		label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	}

	for (QLabel *link : {authorLabel, versionLabel, officialSiteLink, githubLink, communityLink, obsForumLink}) {
		link->setTextFormat(Qt::RichText);
		link->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::TextSelectableByKeyboard);
		link->setOpenExternalLinks(true);
	}

	for (QLabel *separator : {officialSiteSeparator, githubSeparator, communitySeparator}) {
		separator->setText(QStringLiteral("·"));
		separator->setForegroundRole(QPalette::PlaceholderText);
	}

	rootLayout->addStretch();

#ifdef ABOUT_DIALOG_HAS_LEGAL_TEXT
	licensesDialog->setWindowTitle(tr("Licenses"));
	licensesDialog->setModal(false);
	auto *licensesLayout = new QVBoxLayout(licensesDialog);
	licensesTextEdit->setReadOnly(true);
	licensesTextEdit->setPlainText(
		QString::fromUtf8(reinterpret_cast<const char *>(obs_backgroundremoval::legal_text),
				  static_cast<qsizetype>(obs_backgroundremoval::legal_text_size)));
	licensesLayout->addWidget(licensesTextEdit);
	licensesLayout->addWidget(licensesButtonBox);
	connect(licensesButtonBox, &QDialogButtonBox::rejected, licensesDialog, &QDialog::reject);

	licensesButton->setText(tr("Licenses"));
	buttonBox->addButton(licensesButton, QDialogButtonBox::ActionRole);
	connect(licensesButton, &QPushButton::clicked, licensesDialog, [dialog = licensesDialog] {
		dialog->showMaximized();
		dialog->raise();
		dialog->activateWindow();
	});
#endif
	aboutQtButton->setText(tr("About Qt"));
	buttonBox->addButton(aboutQtButton, QDialogButtonBox::ActionRole);
	connect(aboutQtButton, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	rootLayout->addWidget(buttonBox);
}
