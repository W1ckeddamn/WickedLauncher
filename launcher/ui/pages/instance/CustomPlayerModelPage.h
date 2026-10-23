/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#pragma once
#include "ExternalResourcesPage.h"
#include "minecraft/mod/CustomPlayerModelFolderModel.h"

class CustomPlayerModelPage : public ExternalResourcesPage {
    Q_OBJECT
public:
    explicit CustomPlayerModelPage(MinecraftInstance* instance, CustomPlayerModelFolderModel* model, QWidget* parent = nullptr);
    virtual ~CustomPlayerModelPage() = default;

    QString displayName() const override { return tr("Custom Player Models"); }
    QIcon icon() const override { return QIcon::fromTheme("resourcepacks"); }
    QString id() const override { return "customplayermodel"; }
    QString helpPage() const override { return "Custom-Player-Model"; }

    bool shouldDisplay() const override { return true; }

protected slots:
    void renameItem();

private:
    CustomPlayerModelFolderModel* m_model;
};
