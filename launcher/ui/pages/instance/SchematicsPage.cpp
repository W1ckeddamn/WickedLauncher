/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "SchematicsPage.h"
#include <QInputDialog>
#include <QMessageBox>
#include "ui_ExternalResourcesPage.h"

SchematicsPage::SchematicsPage(MinecraftInstance* instance, SchematicsFolderModel* model, QWidget* parent)
    : ExternalResourcesPage(instance, model, parent), m_model(model)
{
    m_ui->actionEnableItem->setVisible(false);
    m_ui->actionDisableItem->setVisible(false);
    m_ui->actionViewConfigs->setVisible(false);
    m_ui->actionViewHomepage->setVisible(false);

    auto renameAction = new QAction(tr("Rename"), this);
    renameAction->setIcon(QIcon::fromTheme("edit-rename"));
    connect(renameAction, &QAction::triggered, this, &SchematicsPage::renameItem);
    m_ui->actionsToolbar->addAction(renameAction);

    m_fileSelectionFilter = "%1 (*.schematic *.schem *.litematic *.nbt *.zip);;All files (*.*)";
}

void SchematicsPage::renameItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    if (selection.indexes().isEmpty()) return;

    auto index = selection.indexes().first();
    Resource& resource = m_model->ResourceFolderModel::at(index.row());
    
    bool ok;
    QString oldName = resource.fileinfo().fileName();
    QString newName = QInputDialog::getText(this, tr("Rename"),
                                            tr("New name:"), QLineEdit::Normal,
                                            oldName, &ok);
    if (ok && !newName.isEmpty() && newName != oldName) {
        QFileInfo fileInfo = resource.fileinfo();
        QString newPath = fileInfo.dir().filePath(newName);
        
        if (QFile::exists(newPath)) {
            QMessageBox::warning(this, tr("Error"), tr("A file with that name already exists."));
            return;
        }

        if (QFile::rename(fileInfo.absoluteFilePath(), newPath)) {
            m_model->update();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to rename file."));
        }
    }
}

