// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#include "ModCheckDialog.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

ModCheckDialog::ModCheckDialog(const QString& modsDir,
                               const QList<ModChecker::Issue>& issues,
                               Mode mode,
                               bool checkEveryLaunch,
                               QWidget* parent)
    : QDialog(parent), m_modsDir(modsDir)
{
    setWindowTitle(mode == Mode::PreLaunch ? tr("Mod check") : tr("Mod check results"));
    setModal(true);
    resize(720, 420);

    auto* layout = new QVBoxLayout(this);

    int errors = 0;
    for (const auto& issue : issues) {
        if (issue.severity == ModChecker::Issue::Severity::Error) {
            errors++;
        }
    }

    auto* header = new QLabel(this);
    header->setWordWrap(true);
    if (errors > 0) {
        header->setText(tr("Found %n problem(s) in the mods folder. Issues marked in red will usually stop the game "
                           "from starting or crash it.",
                           "",
                           static_cast<int>(issues.size())));
    } else {
        header->setText(tr("Found %n warning(s) in the mods folder. The game may still start, but these problems can "
                           "cause crashes later.",
                           "",
                           static_cast<int>(issues.size())));
    }
    layout->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Problem"), tr("Details") });
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    for (const auto& issue : issues) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, issue.summary);
        item->setText(1, issue.details);
        item->setToolTip(1, issue.details);
        if (issue.severity == ModChecker::Issue::Severity::Error) {
            item->setForeground(0, QBrush(QColor(200, 60, 60)));
        } else {
            item->setForeground(0, QBrush(QColor(190, 130, 30)));
        }
    }
    layout->addWidget(m_tree, 1);

    auto* bottom = new QHBoxLayout;
    auto* openFolder = new QPushButton(tr("Open Mods Folder"), this);
    connect(openFolder, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_modsDir)); });
    bottom->addWidget(openFolder);
    bottom->addStretch(1);

    if (mode == Mode::PreLaunch) {
        m_checkBox = new QCheckBox(tr("Check mods before every launch"), this);
        m_checkBox->setChecked(checkEveryLaunch);
        bottom->addWidget(m_checkBox);
    }

    auto* buttons = new QDialogButtonBox(this);
    if (mode == Mode::PreLaunch) {
        auto* launch = buttons->addButton(tr("Launch Anyway"), QDialogButtonBox::AcceptRole);
        launch->setDefault(true);
        buttons->addButton(QDialogButtonBox::Cancel);
    } else {
        buttons->addButton(QDialogButtonBox::Close);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    bottom->addWidget(buttons);

    layout->addLayout(bottom);
}

bool ModCheckDialog::checkEveryLaunch() const
{
    return m_checkBox != nullptr && m_checkBox->isChecked();
}
