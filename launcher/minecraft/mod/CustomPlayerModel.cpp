/*
 *  Luna Launcher - Minecraft Launcher 
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 */
#include "CustomPlayerModel.h"
#include "Json.h"
#include "archive/ArchiveReader.h"
#include <QDebug>

CustomPlayerModel::CustomPlayerModel(QFileInfo file) : Resource(file)
{
    readMetadata();
}

void CustomPlayerModel::readMetadata()
{
    QByteArray data;
    if (m_type == ResourceType::FOLDER) {
        QFile file(m_fileInfo.absoluteDir().filePath(m_fileInfo.fileName() + "/model.json"));
        if (file.open(QIODevice::ReadOnly)) {
            data = file.readAll();
        }
    } else {
        // Zip
        MMCZip::ArchiveReader reader(m_fileInfo.absoluteFilePath());
        auto res = reader.readFile("model.json");
        if (res.has_value()) {
            data = res.value();
        }
    }

    if (data.isEmpty()) {
        return;
    }

    auto docRes = Json::requireDocument(data);
    if (!docRes.has_value()) {
        return;
    }

    auto objRes = Json::requireObject(docRes.value());
    if (!objRes.has_value()) {
        return;
    }

    const auto& obj = objRes.value();

    if (obj.contains("modelName") && obj.value("modelName").isString())
        m_name = obj.value("modelName").toString();
    else
        m_name = m_fileInfo.completeBaseName();

    if (obj.contains("version") && obj.value("version").isString())
        m_version = obj.value("version").toString();
    else
        m_version = "";

    if (obj.contains("author") && obj.value("author").isString())
        m_author = obj.value("author").toString();
    else
        m_author = "";
}
