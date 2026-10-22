// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "EnhancedOfflineDialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QUuid>
#include <QVBoxLayout>

MinecraftAccountPtr createEnhancedOfflineAccount(QWidget* parent, MinecraftAccountPtr existingAccount)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(existingAccount ? QObject::tr("Edit Offline Account") : QObject::tr("Add Enhanced Offline Account"));
    dialog.setMinimumWidth(450);

    auto* layout = new QVBoxLayout(&dialog);

    auto* hint = new QLabel(
        QObject::tr("Create an offline account with a persistent custom UUID and local skin/cape.\n"
                   "When launching, local textures are automatically served to the game."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    layout->addLayout(form);

    auto* nameEdit = new QLineEdit;
    form->addRow(QObject::tr("Player name:"), nameEdit);

    auto* uuidEdit = new QLineEdit;
    uuidEdit->setPlaceholderText(QObject::tr("Leave empty to use standard offline UUID"));
    auto* uuidRow = new QHBoxLayout;
    uuidRow->addWidget(uuidEdit);
    auto* generateUuidBtn = new QPushButton(QObject::tr("Generate"));
    uuidRow->addWidget(generateUuidBtn);
    form->addRow(QObject::tr("Fixed UUID:"), uuidRow);

    QObject::connect(generateUuidBtn, &QPushButton::clicked, &dialog, [uuidEdit] {
        uuidEdit->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
    });

    auto* skinEdit = new QLineEdit;
    auto* skinRow = new QHBoxLayout;
    skinRow->addWidget(skinEdit);
    auto* browseSkinBtn = new QPushButton(QObject::tr("Browse…"));
    skinRow->addWidget(browseSkinBtn);
    form->addRow(QObject::tr("Skin PNG:"), skinRow);

    auto* skinPreview = new QLabel;
    skinPreview->setFixedSize(64, 64);
    skinPreview->setAlignment(Qt::AlignCenter);
    skinPreview->setStyleSheet("border: 1px solid palette(mid); background: palette(base);");
    skinPreview->setText(QObject::tr("No skin"));
    form->addRow(QObject::tr("Preview:"), skinPreview);

    auto* modelCombo = new QComboBox;
    modelCombo->addItem(QObject::tr("Classic (4px arms)"), "classic");
    modelCombo->addItem(QObject::tr("Slim (3px arms, Alex)"), "slim");
    form->addRow(QObject::tr("Skin model:"), modelCombo);

    auto* capeEdit = new QLineEdit;
    auto* capeRow = new QHBoxLayout;
    capeRow->addWidget(capeEdit);
    auto* browseCapeBtn = new QPushButton(QObject::tr("Browse…"));
    capeRow->addWidget(browseCapeBtn);
    form->addRow(QObject::tr("Cape PNG:"), capeRow);

    auto updateSkinPreview = [skinPreview](const QByteArray& data) {
        QPixmap pixmap;
        if (pixmap.loadFromData(data, "PNG") && !pixmap.isNull()) {
            skinPreview->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            skinPreview->setText(QObject::tr("Invalid"));
        }
    };

    QObject::connect(browseSkinBtn, &QPushButton::clicked, &dialog, [&dialog, skinEdit, updateSkinPreview] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QObject::tr("Select Skin PNG"), {}, QObject::tr("PNG images (*.png)"));
        if (!path.isEmpty()) {
            skinEdit->setText(path);
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                updateSkinPreview(file.readAll());
            }
        }
    });

    QObject::connect(browseCapeBtn, &QPushButton::clicked, &dialog, [&dialog, capeEdit] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QObject::tr("Select Cape PNG"), {}, QObject::tr("PNG images (*.png)"));
        if (!path.isEmpty()) {
            capeEdit->setText(path);
        }
    });

    QByteArray existingSkinBytes;
    QByteArray existingCapeBytes;

    if (existingAccount && existingAccount->accountData()) {
        auto* data = existingAccount->accountData();
        nameEdit->setText(data->profileName());
        uuidEdit->setText(data->profileId());
        existingSkinBytes = data->minecraftProfile.skin.data;
        if (!existingSkinBytes.isEmpty()) {
            skinEdit->setPlaceholderText(QObject::tr("<Current skin preserved>"));
            updateSkinPreview(existingSkinBytes);
        }
        if (data->minecraftProfile.skin.variant == "slim") {
            modelCombo->setCurrentIndex(1);
        }
        if (!data->minecraftProfile.capes.isEmpty()) {
            existingCapeBytes = data->minecraftProfile.capes.constBegin().value().data;
            if (!existingCapeBytes.isEmpty()) {
                capeEdit->setPlaceholderText(QObject::tr("<Current cape preserved>"));
            }
        }
    }

    auto* status = new QLabel;
    status->setWordWrap(true);
    status->setStyleSheet("color: palette(highlight);");
    layout->addWidget(status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    MinecraftAccountPtr resultAccount;

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto username = nameEdit->text().trimmed();
        if (username.isEmpty()) {
            status->setText(QObject::tr("Please enter a player name."));
            return;
        }

        QString uuidStr = uuidEdit->text().trimmed();
        if (!uuidStr.isEmpty()) {
            auto parsed = QUuid::fromString(uuidStr);
            if (parsed.isNull() && uuidStr.length() != 32) {
                status->setText(QObject::tr("Invalid UUID format. Leave empty to auto-generate or use standard 32/36 hex format."));
                return;
            }
        }

        QByteArray skinBytes = existingSkinBytes;
        if (!skinEdit->text().isEmpty()) {
            QFile file(skinEdit->text());
            if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
                status->setText(QObject::tr("Cannot read skin file, or file exceeds 2 MiB."));
                return;
            }
            skinBytes = file.readAll();
        }

        QByteArray capeBytes = existingCapeBytes;
        if (!capeEdit->text().isEmpty()) {
            QFile file(capeEdit->text());
            if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
                status->setText(QObject::tr("Cannot read cape file, or file exceeds 2 MiB."));
                return;
            }
            capeBytes = file.readAll();
        }

        QString variant = modelCombo->currentData().toString();

        if (existingAccount) {
            auto* data = existingAccount->accountData();
            data->minecraftProfile.name = username;
            data->yggdrasilToken.extra["userName"] = username;

            if (!uuidStr.isEmpty()) {
                auto parsed = QUuid::fromString(uuidStr);
                data->minecraftProfile.id = !parsed.isNull() ? parsed.toString(QUuid::Id128) : uuidStr.remove('-');
            }

            if (!skinBytes.isEmpty()) {
                data->minecraftProfile.skin.data = skinBytes;
                data->minecraftProfile.skin.variant = variant;
                data->minecraftProfile.skin.id = data->minecraftProfile.id;
                data->minecraftProfile.skin.url = "local";
            }

            if (!capeBytes.isEmpty()) {
                Cape cape;
                cape.id = "local_cape";
                cape.url = "local";
                cape.data = capeBytes;
                data->minecraftProfile.capes[cape.id] = cape;
                data->minecraftProfile.currentCape = cape.id;
            }

            resultAccount = existingAccount;
            emit existingAccount->changed();
        } else {
            resultAccount = MinecraftAccount::createOffline(username, uuidStr, skinBytes, variant, capeBytes);
        }

        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        return resultAccount;
    }
    return nullptr;
}
