/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "YesSteveModelFolderModel.h"
#include <QIcon>

YesSteveModelFolderModel::YesSteveModelFolderModel(const QDir& dir, MinecraftInstance* instance, QObject* parent)
    : ResourceFolderModel(dir, instance, false, true, parent)
{
    m_columnNames = QStringList({ "Name", "Author", "Last Modified", "Size" });
    m_columnNamesTranslated = QStringList({ tr("Name"), tr("Author"), tr("Last Modified"), tr("Size") });
    m_columnSortKeys = { SortType::Name, SortType::Name, SortType::Date, SortType::Size };
    m_columnResizeModes = { QHeaderView::Stretch, QHeaderView::ResizeToContents, QHeaderView::Interactive, QHeaderView::Interactive };
    m_columnsHideable = { false, false, true, true };
}

QVariant YesSteveModelFolderModel::data(const QModelIndex& index, int role) const
{
    if (!validateIndex(index))
        return {};

    int row = index.row();
    int column = index.column();
    auto* resource = dynamic_cast<YesSteveModel*>(m_resources[row].get());
    if(!resource) return {};

    switch (role) {
        case Qt::DisplayRole:
            switch (column) {
                case NameColumn:
                    return resource->name();
                case AuthorColumn:
                    return resource->author();
                case DateColumn:
                    return resource->dateTimeChanged();
                case SizeColumn:
                    return resource->sizeStr();
                default:
                    return {};
            }
        case Qt::DecorationRole: {
            if (column == NameColumn) {
                 // You might want to provide an icon here if available
                 // return QIcon::fromTheme("file");
            }
            return {};
        }
        case Qt::ToolTipRole: {
            if (column == NameColumn) {
                return resource->internalId();
            }
            return {};
        }
        default:
            return {};
    }
}

QVariant YesSteveModelFolderModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section >= 0 && section < m_columnNamesTranslated.size())
            return m_columnNamesTranslated[section];
    }
    return ResourceFolderModel::headerData(section, orientation, role);
}

int YesSteveModelFolderModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}
