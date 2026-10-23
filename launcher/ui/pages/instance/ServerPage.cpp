// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ServerPage.h"

#include <QCheckBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

#include "Application.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/server/ServerInstallTask.h"
#include "ui/dialogs/CreateServerDialog.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
const QString s_eulaUrl = QStringLiteral("https://aka.ms/MinecraftEULA");
}

ServerPage::ServerPage(MinecraftInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    m_manager = new ServerManager(this);
    m_manager->setInstance(instance);
    m_manager->setConfig(ServerManager::loadConfig(m_manager->configPath(), ServerManager::defaultConfig(instance)));

    setupUi();

    connect(m_manager, &ServerManager::outputReady, this, &ServerPage::onOutput);
    connect(m_manager, &ServerManager::runningChanged, this, &ServerPage::onRunningChanged);

    refresh();
}

void ServerPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* folderGroup = new QGroupBox(tr("Server folder"), this);
    auto* folderLayout = new QVBoxLayout(folderGroup);

    auto* folderRow = new QHBoxLayout;
    m_directoryEdit = new QLineEdit(folderGroup);
    m_directoryEdit->setPlaceholderText(tr("Choose or create a server folder"));
    m_browseButton = new QPushButton(tr("Browse…"), folderGroup);
    m_openFolderButton = new QPushButton(tr("Open Folder"), folderGroup);
    folderRow->addWidget(m_directoryEdit, 1);
    folderRow->addWidget(m_browseButton);
    folderRow->addWidget(m_openFolderButton);
    folderLayout->addLayout(folderRow);

    auto* actionRow = new QHBoxLayout;
    m_createButton = new QPushButton(tr("Create Server…"), folderGroup);
    m_importButton = new QPushButton(tr("Import Existing Server…"), folderGroup);
    m_propertiesButton = new QPushButton(tr("server.properties…"), folderGroup);
    actionRow->addWidget(m_createButton);
    actionRow->addWidget(m_importButton);
    actionRow->addWidget(m_propertiesButton);
    actionRow->addStretch(1);
    folderLayout->addLayout(actionRow);

    m_infoLabel = new QLabel(folderGroup);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    folderLayout->addWidget(m_infoLabel);

    layout->addWidget(folderGroup);

    auto* launchGroup = new QGroupBox(tr("Launch settings"), this);
    auto* form = new QFormLayout(launchGroup);

    auto* javaRow = new QHBoxLayout;
    m_javaEdit = new QLineEdit(launchGroup);
    m_javaEdit->setPlaceholderText(tr("Use the launcher's Java"));
    m_javaBrowseButton = new QPushButton(tr("Browse…"), launchGroup);
    javaRow->addWidget(m_javaEdit, 1);
    javaRow->addWidget(m_javaBrowseButton);
    form->addRow(tr("Java"), javaRow);

    m_memorySpin = new QSpinBox(launchGroup);
    m_memorySpin->setRange(256, 1024 * 1024);
    m_memorySpin->setSingleStep(512);
    m_memorySpin->setSuffix(tr(" MB"));
    form->addRow(tr("Memory"), m_memorySpin);

    m_extraArgsEdit = new QLineEdit(launchGroup);
    m_extraArgsEdit->setPlaceholderText(tr("Extra Java arguments (optional)"));
    form->addRow(tr("Java arguments"), m_extraArgsEdit);

    auto* eulaRow = new QHBoxLayout;
    m_eulaCheck = new QCheckBox(tr("I accept the"), launchGroup);
    auto* eulaLink = new QLabel(QStringLiteral("<a href=\"%1\">Minecraft EULA</a>").arg(s_eulaUrl), launchGroup);
    eulaLink->setOpenExternalLinks(true);
    eulaRow->addWidget(m_eulaCheck);
    eulaRow->addWidget(eulaLink);
    eulaRow->addStretch(1);
    form->addRow(tr("EULA"), eulaRow);

    layout->addWidget(launchGroup);

    auto* controlsRow = new QHBoxLayout;
    m_startButton = new QPushButton(tr("Start Server"), this);
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_killButton = new QPushButton(tr("Kill"), this);
    m_stateLabel = new QLabel(tr("Server is stopped."), this);
    controlsRow->addWidget(m_startButton);
    controlsRow->addWidget(m_stopButton);
    controlsRow->addWidget(m_killButton);
    controlsRow->addSpacing(12);
    controlsRow->addWidget(m_stateLabel, 1);
    layout->addLayout(controlsRow);

    auto* consoleGroup = new QGroupBox(tr("Console"), this);
    auto* consoleLayout = new QVBoxLayout(consoleGroup);
    m_console = new QPlainTextEdit(consoleGroup);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(5000);
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_console->setPlaceholderText(tr("Server output appears here."));
    consoleLayout->addWidget(m_console, 1);

    auto* commandRow = new QHBoxLayout;
    m_commandEdit = new QLineEdit(consoleGroup);
    m_commandEdit->setPlaceholderText(tr("Type a server command and press Enter, e.g. say hello"));
    m_sendButton = new QPushButton(tr("Send"), consoleGroup);
    commandRow->addWidget(m_commandEdit, 1);
    commandRow->addWidget(m_sendButton);
    consoleLayout->addLayout(commandRow);

    layout->addWidget(consoleGroup, 1);

    auto* note = new QLabel(tr("The server runs as a child process of the launcher. Closing this instance window or the "
                               "launcher stops it, and its console is not kept between sessions."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);

    connect(m_browseButton, &QPushButton::clicked, this, &ServerPage::browseServerFolder);
    connect(m_openFolderButton, &QPushButton::clicked, this, &ServerPage::openServerFolder);
    connect(m_createButton, &QPushButton::clicked, this, &ServerPage::createServer);
    connect(m_importButton, &QPushButton::clicked, this, &ServerPage::importServer);
    connect(m_propertiesButton, &QPushButton::clicked, this, &ServerPage::editProperties);
    connect(m_javaBrowseButton, &QPushButton::clicked, this, &ServerPage::browseJava);
    connect(m_eulaCheck, &QCheckBox::toggled, this, &ServerPage::onEulaToggled);
    connect(m_startButton, &QPushButton::clicked, this, &ServerPage::startServer);
    connect(m_stopButton, &QPushButton::clicked, this, &ServerPage::stopServer);
    connect(m_killButton, &QPushButton::clicked, this, [this] { m_manager->kill(); });
    connect(m_sendButton, &QPushButton::clicked, this, &ServerPage::sendCommand);
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &ServerPage::sendCommand);
    connect(m_directoryEdit, &QLineEdit::editingFinished, this, [this] {
        collectConfig();
        persist();
        refresh();
    });
    connect(m_javaEdit, &QLineEdit::editingFinished, this, [this] {
        collectConfig();
        persist();
    });
    connect(m_memorySpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        collectConfig();
        persist();
    });
    connect(m_extraArgsEdit, &QLineEdit::editingFinished, this, [this] {
        collectConfig();
        persist();
    });
}

void ServerPage::refresh()
{
    const auto& config = m_manager->config();
    const auto info = ServerManager::inspect(config);
    const bool running = m_manager->isRunning();

    m_directoryEdit->setText(QDir::toNativeSeparators(config.directory));
    m_javaEdit->setText(QDir::toNativeSeparators(config.javaPath));
    {
        const QSignalBlocker blocker(m_memorySpin);
        m_memorySpin->setValue(qMax(256, config.memoryMb));
    }

    QStringList quotedArguments;
    for (const auto& argument : config.extraArgs) {
        quotedArguments.append(argument.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(argument) : argument);
    }
    m_extraArgsEdit->setText(quotedArguments.join(QLatin1Char(' ')));

    if (!info.valid) {
        m_infoLabel->setText(info.warnings.isEmpty() ? tr("No server is configured for this instance yet.")
                                                     : info.warnings.join(QLatin1Char('\n')));
    } else {
        const auto version = info.config.minecraftVersion.isEmpty() ? tr("version unknown") : info.config.minecraftVersion;
        const auto source = info.config.jar.isEmpty() ? tr("Forge arguments") : info.config.jar;
        auto text = tr("%1 %2 — %3").arg(ServerManager::kindName(info.config.kind), version, source);
        if (!info.warnings.isEmpty()) {
            text.append(QLatin1Char('\n'));
            text.append(info.warnings.join(QLatin1Char('\n')));
        }
        m_infoLabel->setText(text);
    }

    {
        const QSignalBlocker blocker(m_eulaCheck);
        m_eulaCheck->setChecked(info.eulaAccepted);
    }

    m_startButton->setEnabled(info.valid && !running);
    m_stopButton->setEnabled(running);
    m_killButton->setEnabled(running);
    m_sendButton->setEnabled(running);
    m_commandEdit->setEnabled(running);
    m_stateLabel->setText(running ? tr("Server is running.") : tr("Server is stopped."));
}

void ServerPage::collectConfig()
{
    auto config = m_manager->config();
    const auto directory = m_directoryEdit->text().trimmed();
    config.directory = directory.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(directory));
    config.javaPath = QDir::fromNativeSeparators(m_javaEdit->text().trimmed());
    config.memoryMb = m_memorySpin->value();
    config.extraArgs = QProcess::splitCommand(m_extraArgsEdit->text());
    m_manager->setConfig(config);
}

void ServerPage::persist()
{
    QString error;
    if (!m_manager->persist(&error) && !error.isEmpty()) {
        qWarning() << "Could not save the server settings:" << error;
    }
}

bool ServerPage::apply()
{
    collectConfig();
    persist();
    return true;
}

void ServerPage::openedImpl()
{
    refresh();
}

void ServerPage::closedImpl()
{
    collectConfig();
    persist();
}

void ServerPage::createServer()
{
    CreateServerDialog dialog(m_instance, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto config = dialog.config();
    std::unique_ptr<ServerInstallTask> task(
        new ServerInstallTask(APPLICATION->network(), config.directory, config.kind, config.minecraftVersion));

    ProgressDialog progress(this);
    progress.execWithTask(task.get());

    if (!task->wasSuccessful()) {
        if (!task->failReason().isEmpty()) {
            QMessageBox::critical(this, tr("Could not create the server"), task->failReason());
        }
        return;
    }

    auto saved = config;
    saved.jar = task->jarName();
    saved.argsFile = task->argsFile();
    saved.minecraftVersion = config.minecraftVersion;

    QString error;
    if (!ServerManager::saveConfig(m_manager->configPath(), saved, &error)) {
        QMessageBox::warning(this, tr("Could not save the settings"), error);
    }
    m_manager->setConfig(saved);

    appendConsole(tr("[launcher] Created a %1 %2 server in %3")
                      .arg(ServerManager::kindName(saved.kind), saved.minecraftVersion, saved.directory));
    refresh();
}

void ServerPage::importServer()
{
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Choose an existing server folder"),
                                                             m_manager->config().directory);
    if (directory.isEmpty()) {
        return;
    }

    auto config = m_manager->config();
    config.directory = QDir::cleanPath(QDir::fromNativeSeparators(directory));
    config.jar.clear();
    config.argsFile.clear();
    config.minecraftVersion.clear();
    config.kind = ServerManager::Kind::Unknown;

    const auto info = ServerManager::inspect(config);
    if (!info.valid) {
        QMessageBox::warning(this, tr("No server jar found"),
                             info.warnings.join(QLatin1Char('\n')) +
                                 tr("\n\nThe folder is still selected, so you can add the server files yourself."));
    } else {
        config = info.config;
    }

    m_manager->setConfig(config);
    persist();
    refresh();
    appendConsole(tr("[launcher] Imported the server folder %1").arg(config.directory));
}

void ServerPage::editProperties()
{
    collectConfig();
    const auto directory = m_manager->config().directory;
    if (directory.isEmpty() || !QFileInfo::exists(directory)) {
        QMessageBox::information(this, tr("No server folder"), tr("Create or import a server first."));
        return;
    }

    const auto path = FS::PathCombine(directory, QStringLiteral("server.properties"));
    QString contents;
    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Could not read server.properties"), file.errorString());
            return;
        }
        contents = QString::fromUtf8(file.readAll());
    } else {
        contents = QStringLiteral("# server.properties\n"
                                  "# The server creates this file on its first start; these defaults are a safe start.\n"
                                  "server-port=25565\n"
                                  "motd=A Minecraft Server\n"
                                  "online-mode=true\n");
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("server.properties"));
    dialog.resize(560, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(tr("One setting per line, as key=value. Changes take effect after the next restart."), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* editor = new QPlainTextEdit(contents, &dialog);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(editor, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Could not save server.properties"), output.errorString());
        return;
    }
    output.write(editor->toPlainText().toUtf8());
    if (!output.commit()) {
        QMessageBox::warning(this, tr("Could not save server.properties"), output.errorString());
        return;
    }
    appendConsole(tr("[launcher] Saved server.properties"));
}

void ServerPage::browseServerFolder()
{
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Choose the server folder"),
                                                             m_manager->config().directory);
    if (directory.isEmpty()) {
        return;
    }
    m_directoryEdit->setText(QDir::toNativeSeparators(directory));
    collectConfig();
    persist();
    refresh();
}

void ServerPage::openServerFolder()
{
    const auto directory = m_manager->config().directory;
    if (directory.isEmpty() || !QFileInfo::exists(directory)) {
        QMessageBox::information(this, tr("No server folder"), tr("The server folder does not exist yet."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void ServerPage::browseJava()
{
    const auto java = QFileDialog::getOpenFileName(this, tr("Choose the Java executable"), m_javaEdit->text());
    if (java.isEmpty()) {
        return;
    }
    m_javaEdit->setText(QDir::toNativeSeparators(java));
    collectConfig();
    persist();
}

void ServerPage::onEulaToggled(bool accepted)
{
    const auto directory = m_manager->config().directory;
    if (directory.isEmpty() || !QFileInfo::exists(directory)) {
        if (accepted) {
            QMessageBox::information(this, tr("No server folder"),
                                     tr("Create or import a server before accepting the EULA."));
        }
        const QSignalBlocker blocker(m_eulaCheck);
        m_eulaCheck->setChecked(false);
        return;
    }
    if (accepted) {
        const auto answer = QMessageBox::question(
            this, tr("Minecraft EULA"),
            tr("Do you accept the Minecraft server EULA?\n\n%1\n\nThe server only starts after it has been accepted.").arg(s_eulaUrl));
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker blocker(m_eulaCheck);
            m_eulaCheck->setChecked(false);
            return;
        }
    }
    ServerManager::writeEula(directory, accepted);
    refresh();
}

void ServerPage::startServer()
{
    collectConfig();
    const auto config = m_manager->config();

    if (config.directory.isEmpty() || !QFileInfo::exists(config.directory)) {
        QMessageBox::warning(this, tr("No server folder"),
                             tr("Create a server or import an existing one before starting it."));
        return;
    }

    const auto info = ServerManager::inspect(config);
    if (!info.valid) {
        QMessageBox::warning(this, tr("No server jar"), info.warnings.join(QLatin1Char('\n')));
        return;
    }
    m_manager->setConfig(info.config);
    persist();

    if (!info.eulaAccepted) {
        const auto answer = QMessageBox::question(
            this, tr("Minecraft EULA"),
            tr("The server can only start after the Minecraft EULA is accepted.\n\n%1\n\nAccept it now?").arg(s_eulaUrl));
        if (answer != QMessageBox::Yes) {
            return;
        }
        ServerManager::writeEula(info.config.directory, true);
    }

    QString error;
    if (!m_manager->start(&error)) {
        QMessageBox::critical(this, tr("Could not start the server"), error);
        refresh();
        return;
    }
    persist();
    refresh();
}

void ServerPage::stopServer()
{
    m_manager->stop();
}

void ServerPage::sendCommand()
{
    const auto command = m_commandEdit->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    m_manager->sendCommand(command);
    appendConsole(QStringLiteral("> ") + command);
    m_commandEdit->clear();
}

void ServerPage::onOutput(const QString& text)
{
    appendConsole(text);
}

void ServerPage::onRunningChanged(bool running)
{
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_killButton->setEnabled(running);
    m_sendButton->setEnabled(running);
    m_commandEdit->setEnabled(running);
    m_stateLabel->setText(running ? tr("Server is running.") : tr("Server is stopped."));
}

void ServerPage::appendConsole(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    m_console->moveCursor(QTextCursor::End);
    m_console->insertPlainText(text);
    if (!text.endsWith(QLatin1Char('\n'))) {
        m_console->insertPlainText(QStringLiteral("\n"));
    }
    m_console->moveCursor(QTextCursor::End);
}
