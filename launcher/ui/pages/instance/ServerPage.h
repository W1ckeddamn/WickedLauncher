// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QWidget>

#include "minecraft/server/ServerManager.h"
#include "ui/pages/BasePage.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class MinecraftInstance;

/**
 * Per-instance page for a dedicated server.
 *
 * The page can create a new server folder, adopt an existing one, edit its launch settings and run
 * it with a console. Everything the page knows is stored in <instance>/wicked-server.json, so the
 * server folder itself can stay wherever the user put it.
 */
class ServerPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit ServerPage(MinecraftInstance* instance, QWidget* parent = nullptr);
    ~ServerPage() override = default;

    QString id() const override { return QStringLiteral("dedicatedserver"); }
    QString displayName() const override { return tr("Dedicated Server"); }
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("server")); }

    bool apply() override;
    void openedImpl() override;
    void closedImpl() override;

   private slots:
    void createServer();
    void importServer();
    void editProperties();
    void browseServerFolder();
    void openServerFolder();
    void browseJava();
    void onEulaToggled(bool accepted);
    void startServer();
    void stopServer();
    void sendCommand();
    void onOutput(const QString& text);
    void onRunningChanged(bool running);
    void refresh();

   private:
    void setupUi();
    void collectConfig();
    void persist();
    void appendConsole(const QString& text);

    MinecraftInstance* m_instance = nullptr;
    ServerManager* m_manager = nullptr;

    QLineEdit* m_directoryEdit = nullptr;
    QLabel* m_infoLabel = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_propertiesButton = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_openFolderButton = nullptr;

    QLineEdit* m_javaEdit = nullptr;
    QPushButton* m_javaBrowseButton = nullptr;
    QSpinBox* m_memorySpin = nullptr;
    QLineEdit* m_extraArgsEdit = nullptr;
    QCheckBox* m_eulaCheck = nullptr;

    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_killButton = nullptr;
    QLabel* m_stateLabel = nullptr;

    QPlainTextEdit* m_console = nullptr;
    QLineEdit* m_commandEdit = nullptr;
    QPushButton* m_sendButton = nullptr;
};
