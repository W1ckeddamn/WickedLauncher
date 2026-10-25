// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QDialog>

#include "minecraft/server/ServerManager.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkReply;
class QPushButton;
class QSpinBox;
class MinecraftInstance;

/**
 * Collects everything needed to create a new server folder.
 *
 * The Minecraft version list is fetched from Mojang in the background; while it loads (or when the
 * request fails) the version box stays editable so the version can be typed by hand.
 */
class CreateServerDialog : public QDialog {
    Q_OBJECT

   public:
    explicit CreateServerDialog(MinecraftInstance* instance, QWidget* parent = nullptr);
    ~CreateServerDialog() override;

    ServerManager::Config config() const;

   public slots:
    void accept() override;

   private slots:
    void loadVersions();
    void onVersionsLoaded();
    void browseDirectory();
    void updateState();

   private:
    MinecraftInstance* m_instance = nullptr;

    QComboBox* m_kindCombo = nullptr;
    QComboBox* m_versionCombo = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QSpinBox* m_memorySpin = nullptr;
    QLabel* m_descriptionLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_okButton = nullptr;
    QNetworkReply* m_versionsReply = nullptr;
};
