// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "CreateServerDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

namespace {
const QString s_manifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
}  // namespace

CreateServerDialog::CreateServerDialog(MinecraftInstance* instance, QWidget* parent) : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Create a server"));
    setModal(true);
    resize(520, 360);

    const auto defaults = ServerManager::defaultConfig(instance);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(tr("Vanilla"), static_cast<int>(ServerManager::Kind::Vanilla));
    m_kindCombo->addItem(tr("Fabric"), static_cast<int>(ServerManager::Kind::Fabric));
    m_kindCombo->addItem(tr("Paper"), static_cast<int>(ServerManager::Kind::Paper));
    m_kindCombo->addItem(tr("Purpur"), static_cast<int>(ServerManager::Kind::Purpur));
    form->addRow(tr("Server type"), m_kindCombo);

    m_versionCombo = new QComboBox(this);
    m_versionCombo->setEditable(true);
    m_versionCombo->addItem(tr("Loading…"));
    form->addRow(tr("Minecraft version"), m_versionCombo);

    auto* directoryRow = new QHBoxLayout;
    m_directoryEdit = new QLineEdit(defaults.directory, this);
    auto* browse = new QPushButton(tr("Browse…"), this);
    directoryRow->addWidget(m_directoryEdit, 1);
    directoryRow->addWidget(browse);
    form->addRow(tr("Server folder"), directoryRow);

    m_memorySpin = new QSpinBox(this);
    m_memorySpin->setRange(256, 1024 * 1024);
    m_memorySpin->setSingleStep(512);
    m_memorySpin->setSuffix(tr(" MB"));
    m_memorySpin->setValue(qMax(512, defaults.memoryMb));
    form->addRow(tr("Memory"), m_memorySpin);

    auto* javaRow = new QHBoxLayout;
    m_javaEdit = new QLineEdit(defaults.javaPath, this);
    m_javaEdit->setPlaceholderText(tr("Use the launcher's Java"));
    javaRow->addWidget(m_javaEdit, 1);
    form->addRow(tr("Java"), javaRow);

    layout->addLayout(form);

    m_statusLabel = new QLabel(tr("Forge and NeoForge servers need their official installer; create one there and use "
                                  "\"Import Server\" instead."),
                               this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    layout->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    m_okButton = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &CreateServerDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, &CreateServerDialog::browseDirectory);
    connect(m_versionCombo, &QComboBox::currentTextChanged, this, [this] { m_okButton->setEnabled(!config().minecraftVersion.isEmpty()); });
    connect(m_directoryEdit, &QLineEdit::textChanged, this, [this] { m_okButton->setEnabled(!config().minecraftVersion.isEmpty()); });

    loadVersions();
}

CreateServerDialog::~CreateServerDialog()
{
    if (m_versionsReply != nullptr) {
        m_versionsReply->abort();
        m_versionsReply->deleteLater();
        m_versionsReply = nullptr;
    }
}

ServerManager::Config CreateServerDialog::config() const
{
    ServerManager::Config config;
    config.kind = static_cast<ServerManager::Kind>(m_kindCombo->currentData().toInt());
    config.minecraftVersion = m_versionCombo->currentText().trimmed();
    if (config.minecraftVersion == tr("Loading…")) {
        config.minecraftVersion.clear();
    }
    config.directory = m_directoryEdit->text().trimmed();
    if (!config.directory.isEmpty()) {
        config.directory = QDir::cleanPath(QDir::fromNativeSeparators(config.directory));
    }
    config.memoryMb = m_memorySpin->value();
    config.javaPath = m_javaEdit->text().trimmed();
    return config;
}

void CreateServerDialog::loadVersions()
{
    QNetworkRequest request(QUrl(s_manifestUrl));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);

    m_versionsReply = APPLICATION->network()->get(request);
    connect(m_versionsReply, &QNetworkReply::finished, this, &CreateServerDialog::onVersionsLoaded);
}

void CreateServerDialog::onVersionsLoaded()
{
    auto* reply = m_versionsReply;
    m_versionsReply = nullptr;
    if (reply == nullptr) {
        return;
    }
    const auto data = reply->readAll();
    const auto error = reply->errorString();
    const auto errorCode = reply->error();
    reply->deleteLater();

    if (errorCode != QNetworkReply::NoError) {
        m_versionCombo->clear();
        m_versionCombo->setEditable(true);
        m_statusLabel->setText(tr("Could not load the version list (%1). Type a Minecraft version by hand.").arg(error));
        return;
    }

    const auto document = QJsonDocument::fromJson(data);
    const auto versions = document.object().value(QStringLiteral("versions")).toArray();
    if (versions.isEmpty()) {
        m_statusLabel->setText(tr("Mojang returned an empty version list. Type a Minecraft version by hand."));
        m_versionCombo->clear();
        return;
    }

    QString preferred;
    if (m_instance != nullptr) {
        if (auto* profile = m_instance->getPackProfile(); profile != nullptr) {
            preferred = profile->getComponentVersion(QStringLiteral("net.minecraft"));
        }
    }

    m_versionCombo->blockSignals(true);
    m_versionCombo->clear();
    for (const auto& value : versions) {
        const auto version = value.toObject();
        if (version.value(QStringLiteral("type")).toString() != QStringLiteral("release")) {
            continue;
        }
        m_versionCombo->addItem(version.value(QStringLiteral("id")).toString());
    }
    m_versionCombo->blockSignals(false);

    if (!preferred.isEmpty()) {
        const int index = m_versionCombo->findText(preferred);
        if (index >= 0) {
            m_versionCombo->setCurrentIndex(index);
        } else {
            m_versionCombo->setEditText(preferred);
        }
    } else if (m_versionCombo->count() > 0) {
        m_versionCombo->setCurrentIndex(0);
    }

    m_okButton->setEnabled(!config().minecraftVersion.isEmpty());
}

void CreateServerDialog::browseDirectory()
{
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Choose the server folder"), m_directoryEdit->text());
    if (!directory.isEmpty()) {
        m_directoryEdit->setText(QDir::toNativeSeparators(directory));
    }
}

void CreateServerDialog::accept()
{
    const auto selected = config();
    if (selected.minecraftVersion.isEmpty()) {
        QMessageBox::warning(this, tr("No version selected"), tr("Choose the Minecraft version to install."));
        return;
    }
    if (selected.directory.isEmpty()) {
        QMessageBox::warning(this, tr("No folder selected"), tr("Choose where the server files should be stored."));
        return;
    }

    if (QFileInfo::exists(QDir(selected.directory).filePath(QStringLiteral("server.jar")))) {
        const auto answer = QMessageBox::question(
            this, tr("Replace the existing server?"),
            tr("The folder already contains a server.jar. Creating a new server will replace it.\n\nDo you want to continue?"));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QDialog::accept();
}
