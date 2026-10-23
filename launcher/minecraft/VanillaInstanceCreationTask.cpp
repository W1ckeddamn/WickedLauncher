#include "VanillaInstanceCreationTask.h"

#include <utility>

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

VanillaCreationTask::VanillaCreationTask(BaseVersion::Ptr version, QString loader, BaseVersion::Ptr loaderVersion)
    : m_version(std::move(version)), m_usingLoader(true), m_loader(std::move(loader)), m_loaderVersion(std::move(loaderVersion))
{}

void VanillaCreationTask::executeTask()
{
    setStatus(tr("Creating instance from version %1").arg(m_version->name()));

    m_instance = std::make_unique<MinecraftInstance>(
        m_globalSettings, std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")), m_stagingPath);
    {
        const SettingsObject::Lock lock(m_instance->settings());

        auto* components = m_instance->getPackProfile();
        components->buildingFromScratch();
        components->setComponentVersion("net.minecraft", m_version->descriptor(), true);
        if (m_usingLoader) {
            components->setComponentVersion(m_loader, m_loaderVersion->descriptor());
        }

        m_instance->setName(name());
        const bool defaultIcon = m_instIcon.isEmpty() || m_instIcon == QStringLiteral("default");
        m_instance->setIconKey((m_serverOnly && defaultIcon) ? QStringLiteral("server") : m_instIcon);
        m_instance->setServerOnly(m_serverOnly);

        components->saveNow();
    }

    if (m_serverOnly) {
        // A dedicated server gets its jar from the server page; client game files are not needed.
        emitSucceeded();
        return;
    }

    downloadFiles(m_instance.get());
}
