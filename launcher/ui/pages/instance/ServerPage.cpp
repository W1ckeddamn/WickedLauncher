// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ServerPage.h"

#include <QCheckBox>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
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
#include <QStackedWidget>
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

    layout->addWidget(buildHeader());

    auto* toolbar = new QHBoxLayout;
    m_createButton = new QPushButton(tr("Create Server…"), this);
    m_createButton->setToolTip(tr("Download a new server (Vanilla, Paper, Purpur or Fabric)"));
    m_importButton = new QPushButton(tr("Import Server…"), this);
    m_importButton->setToolTip(tr("Use an existing server folder, for example a Forge or NeoForge server"));
    m_openFolderButton = new QPushButton(tr("Open Folder"), this);
    m_propertiesButton = new QPushButton(tr("server.properties…"), this);
    toolbar->addWidget(m_createButton);
    toolbar->addWidget(m_importButton);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_openFolderButton);
    toolbar->addWidget(m_propertiesButton);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);

    m_stack = new QStackedWidget(this);
    m_emptyPage = buildEmptyPage();
    m_settingsPage = buildSettings();
    m_stack->addWidget(m_emptyPage);
    m_stack->addWidget(m_settingsPage);
    layout->addWidget(m_stack, 1);

    connect(m_createButton, &QPushButton::clicked, this, &ServerPage::createServer);
    connect(m_importButton, &QPushButton::clicked, this, &ServerPage::importServer);
    connect(m_openFolderButton, &QPushButton::clicked, this, &ServerPage::openServerFolder);
    connect(m_propertiesButton, &QPushButton::clicked, this, &ServerPage::editProperties);
}

QWidget* ServerPage::buildHeader()
{
    auto* header = new QFrame(this);
    header->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(header);

    auto* topRow = new QHBoxLayout;
    m_stateLabel = new QLabel(header);
    auto font = m_stateLabel->font();
    font.setBold(true);
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * 1.2);
    }
    m_stateLabel->setFont(font);
    m_identityLabel = new QLabel(header);
    topRow->addWidget(m_stateLabel);
    topRow->addWidget(m_identityLabel);
    topRow->addStretch(1);

    m_startButton = new QPushButton(tr("Start"), header);
    m_stopButton = new QPushButton(tr("Stop"), header);
    m_killButton = new QPushButton(tr("Kill"), header);
    m_killButton->setToolTip(tr("Kill the server process without saving the world"));
    topRow->addWidget(m_startButton);
    topRow->addWidget(m_stopButton);
    topRow->addWidget(m_killButton);
    layout->addLayout(topRow);

    auto* addressRow = new QHBoxLayout;
    auto* addressTitle = new QLabel(tr("Address:"), header);
    m_addressLabel = new QLabel(header);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_copyAddressButton = new QPushButton(tr("Copy"), header);
    m_copyAddressButton->setToolTip(tr("Copy the address players can connect to"));
    addressRow->addWidget(addressTitle);
    addressRow->addWidget(m_addressLabel);
    addressRow->addWidget(m_copyAddressButton);
    addressRow->addStretch(1);
    layout->addLayout(addressRow);

    m_warningLabel = new QLabel(header);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_warningLabel);

    connect(m_startButton, &QPushButton::clicked, this, &ServerPage::startServer);
    connect(m_stopButton, &QPushButton::clicked, this, &ServerPage::stopServer);
    connect(m_killButton, &QPushButton::clicked, this, [this] { m_manager->kill(); });
    connect(m_copyAddressButton, &QPushButton::clicked, this, &ServerPage::copyAddress);

    return header;
}

QWidget* ServerPage::buildEmptyPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addStretch(1);

    const auto pixmap = QIcon::fromTheme(QStringLiteral("server")).pixmap(64, 64);
    if (!pixmap.isNull()) {
        auto* icon = new QLabel(page);
        icon->setPixmap(pixmap);
        icon->setAlignment(Qt::AlignCenter);
        layout->addWidget(icon);
    }

    auto* title = new QLabel(tr("No server yet"), page);
    auto titleFont = title->font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0) {
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    }
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* hint = new QLabel(tr("Use <b>Create Server</b> to download a Vanilla, Paper, Purpur or Fabric server, or "
                               "<b>Import Server</b> to use a server folder you already have."),
                            page);
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    layout->addStretch(2);
    return page;
}

QWidget* ServerPage::buildSettings()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* folderGroup = new QGroupBox(tr("Server folder"), page);
    auto* folderRow = new QHBoxLayout(folderGroup);
    m_directoryEdit = new QLineEdit(folderGroup);
    m_directoryEdit->setPlaceholderText(tr("Choose or create a server folder"));
    m_browseButton = new QPushButton(tr("Browse…"), folderGroup);
    folderRow->addWidget(m_directoryEdit, 1);
    folderRow->addWidget(m_browseButton);
    layout->addWidget(folderGroup);

    auto* launchGroup = new QGroupBox(tr("Java and launch options"), page);
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
    layout->addWidget(buildConsole(), 1);

    auto* note = new QLabel(tr("The server runs as a child process of the launcher. Closing this instance window stops it, "
                               "and its console is not kept between sessions."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);

    connect(m_browseButton, &QPushButton::clicked, this, &ServerPage::browseServerFolder);
    connect(m_javaBrowseButton, &QPushButton::clicked, this, &ServerPage::browseJava);
    connect(m_eulaCheck, &QCheckBox::toggled, this, &ServerPage::onEulaToggled);
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

    return page;
}

QWidget* ServerPage::buildConsole()
{
    auto* group = new QGroupBox(tr("Console"), this);
    auto* layout = new QVBoxLayout(group);

    auto* toolRow = new QHBoxLayout;
    toolRow->addStretch(1);
    m_clearConsoleButton = new QPushButton(tr("Clear"), group);
    toolRow->addWidget(m_clearConsoleButton);
    layout->addLayout(toolRow);

    m_console = new QPlainTextEdit(group);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(5000);
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_console->setPlaceholderText(tr("Server output appears here."));
    layout->addWidget(m_console, 1);

    auto* commandRow = new QHBoxLayout;
    m_commandEdit = new QLineEdit(group);
    m_commandEdit->setPlaceholderText(tr("Type a server command and press Enter, e.g. say hello"));
    m_sendButton = new QPushButton(tr("Send"), group);
    commandRow->addWidget(m_commandEdit, 1);
    commandRow->addWidget(m_sendButton);
    layout->addLayout(commandRow);

    connect(m_clearConsoleButton, &QPushButton::clicked, this, &ServerPage::clearConsole);
    connect(m_sendButton, &QPushButton::clicked, this, &ServerPage::sendCommand);
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &ServerPage::sendCommand);

    return group;
}

QString ServerPage::listenAddress() const
{
    QString port = QStringLiteral("25565");
    const auto directory = m_manager->config().directory;
    if (directory.isEmpty()) {
        return {};
    }

    QFile file(FS::PathCombine(directory, QStringLiteral("server.properties")));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (const auto& line : lines) {
            const auto entry = line.trimmed();
            if (entry.startsWith(QStringLiteral("server-port="))) {
                const auto value = entry.section(QLatin1Char('='), 1).trimmed();
                if (!value.isEmpty()) {
                    port = value;
                }
                break;
            }
        }
    }
    return QStringLiteral("localhost:%1").arg(port);
}

void ServerPage::refresh()
{
    const auto& config = m_manager->config();
    const auto info = ServerManager::inspect(config);
    const bool running = m_manager->isRunning();
    const bool hasFolder = !config.directory.isEmpty() && QFileInfo::exists(config.directory);

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

    {
        const QSignalBlocker blocker(m_eulaCheck);
        m_eulaCheck->setChecked(info.eulaAccepted);
    }

    if (info.valid) {
        const auto version = info.config.minecraftVersion.isEmpty() ? tr("version unknown") : info.config.minecraftVersion;
        m_identityLabel->setText(tr("%1 %2").arg(ServerManager::kindName(info.config.kind), version));
        m_identityLabel->setToolTip(info.config.jar.isEmpty() ? tr("Launched through the argument file")
                                                              : tr("Server jar: %1").arg(info.config.jar));
    } else if (hasFolder) {
        m_identityLabel->setText(tr("No server files found"));
        m_identityLabel->setToolTip({});
    } else {
        m_identityLabel->setText(tr("No server configured"));
        m_identityLabel->setToolTip({});
    }

    m_addressLabel->setText(hasFolder ? listenAddress() : QString());
    m_copyAddressButton->setEnabled(hasFolder);
    m_warningLabel->setText(hasFolder ? info.warnings.join(QLatin1Char('\n')) : QString());
    m_warningLabel->setVisible(hasFolder && !info.warnings.isEmpty());

    m_stack->setCurrentWidget(hasFolder ? m_settingsPage : m_emptyPage);

    m_stateLabel->setText(running ? tr("Running") : tr("Stopped"));
    m_startButton->setEnabled(info.valid && !running);
    m_stopButton->setEnabled(running);
    m_killButton->setEnabled(running);
    m_createButton->setEnabled(!running);
    m_importButton->setEnabled(!running);
    m_openFolderButton->setEnabled(hasFolder);
    m_propertiesButton->setEnabled(hasFolder);
    m_directoryEdit->setEnabled(!running);
    m_browseButton->setEnabled(!running);
    m_eulaCheck->setEnabled(!running);
    m_sendButton->setEnabled(running);
    m_commandEdit->setEnabled(running);
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
    refresh();
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

void ServerPage::copyAddress()
{
    const auto address = listenAddress();
    if (!address.isEmpty()) {
        QGuiApplication::clipboard()->setText(address);
    }
}

void ServerPage::clearConsole()
{
    m_console->clear();
}

void ServerPage::onOutput(const QString& text)
{
    appendConsole(text);
}

void ServerPage::onRunningChanged(bool running)
{
    Q_UNUSED(running)
    refresh();
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
