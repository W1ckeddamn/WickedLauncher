#include "YggdrasilMinecraftProfileStep.h"

#include <QNetworkRequest>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

YggdrasilMinecraftProfileStep::YggdrasilMinecraftProfileStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilMinecraftProfileStep::describe()
{
    return tr("Fetching the Minecraft profile.");
}

void YggdrasilMinecraftProfileStep::perform()
{
    if (m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("A UUID is required to get the profile."));
        return;
    }

    QUrl url = QUrl(m_data->sessionServerUrl() + "/session/minecraft/profile/" + m_data->minecraftProfile.id);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" }, { "Accept", "application/json" } };

    auto [request, response] = Net::Request::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("MinecraftProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void YggdrasilMinecraftProfileStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() != QNetworkReply::NoError || !response) {
        qWarning() << "Error getting profile:" << m_request->errorString();
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                      tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        return;
    }
    if (!Parsers::parseMinecraftProfileMojang(*response, m_data->minecraftProfile)) {
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft Java profile response could not be parsed"));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Minecraft Java profile acquisition succeeded."));
}
