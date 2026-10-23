// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include <QDialog>

#include "minecraft/mod/ModChecker.h"

class QCheckBox;
class QTreeWidget;

/**
 * Shows the issues found by ModChecker.
 *
 * In PreLaunch mode the dialog offers "Launch Anyway" and remembers whether the check should run
 * before every launch. In Review mode it is informational only and is opened from the launch menu.
 */
class ModCheckDialog : public QDialog {
    Q_OBJECT

   public:
    enum class Mode {
        PreLaunch,
        Review,
    };

    ModCheckDialog(const QString& modsDir,
                   const QList<ModChecker::Issue>& issues,
                   Mode mode,
                   bool checkEveryLaunch,
                   QWidget* parent = nullptr);

    bool checkEveryLaunch() const;

   private:
    QString m_modsDir;
    QCheckBox* m_checkBox = nullptr;
    QTreeWidget* m_tree = nullptr;
};
