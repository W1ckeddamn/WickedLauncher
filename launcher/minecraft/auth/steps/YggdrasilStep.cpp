#include "YggdrasilStep.h"
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "Application.h"
#include "net/RawHeaderProxy.h"

YggdrasilStep::YggdrasilStep(AccountData* data, const std::optional<QString>& password)
    : AuthStep(data), m_password(password)
{
}

QString YggdrasilStep::describe()
{
    return tr("Logging in with Yggdrasil.");
}

void YggdrasilStep::perform()
{
    if (m_password) {
        login(*m_password);
    } else {
        refresh();
    }
}

void YggdrasilStep::login(const QString& password)
{
    QUrl url = QUrl(m_data->authServerUrl() + "/authenticate");
    QJsonObject requestData;
    QJsonObject agent;
    agent.insert("name", "Minecraft");
    agent.insert("version", 1);
    requestData.insert("agent", agent);
    requestData.insert("username", m_data->userName());
    requestData.insert("password", password);
    requestData.insert("clientToken", m_data->clientToken());
    requestData.insert("requestUser", true);

    QByteArray uploadData = QJsonDocument(requestData).toJson(QJsonDocument::Compact);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" }, { "Accept", "application/json" } };

    auto [request, response] = Net::Request::makeByteArray(url, uploadData);
    request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request = request;

    m_task.reset(new NetJob("YggdrasilStep::login", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });
    m_task->start();
}

void YggdrasilStep::refresh()
{
    QUrl url = QUrl(m_data->authServerUrl() + "/refresh");
    QJsonObject requestData;
    requestData.insert("accessToken", m_data->accessToken());
    requestData.insert("clientToken", m_data->clientToken());
    requestData.insert("requestUser", true);

    if (m_didSelectProfile && !m_data->minecraftProfile.id.isEmpty() && !m_data->minecraftProfile.name.isEmpty()) {
        QJsonObject selectedProfile;
        selectedProfile.insert("id", m_data->minecraftProfile.id);
        selectedProfile.insert("name", m_data->minecraftProfile.name);
        requestData.insert("selectedProfile", selectedProfile);
    }

    QByteArray uploadData = QJsonDocument(requestData).toJson(QJsonDocument::Compact);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" }, { "Accept", "application/json" } };

    auto [request, response] = Net::Request::makeByteArray(url, uploadData);
    request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request = request;

    m_task.reset(new NetJob("YggdrasilStep::refresh", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });
    m_task->start();
}

void YggdrasilStep::onRequestDone(QByteArray* response)
{
    if (!response || response->isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Authentication server returned an empty response."));
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Could not parse JSON from authentication server."));
        return;
    }

    QJsonObject responseData = doc.object();
    if (m_request->error() != QNetworkReply::NoError) {
        processError(responseData);
        return;
    }

    processResponse(responseData);
}

void YggdrasilStep::processResponse(const QJsonObject& responseData)
{
    auto accessTokenVal = responseData.value("accessToken");
    if (!accessTokenVal.isString()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication server didn't send an access token."));
        return;
    }
    m_data->yggdrasilToken.token = accessTokenVal.toString();

    auto clientTokenVal = responseData.value("clientToken");
    if (clientTokenVal.isString()) {
        m_data->setClientToken(clientTokenVal.toString());
    }

    QJsonValue profile = responseData.value("selectedProfile");
    if (!profile.isObject()) {
        auto profiles = responseData.value("availableProfiles");
        if (!profiles.isArray() || profiles.toArray().isEmpty()) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Account has no available profile."));
            return;
        }

        const auto& profilesArray = profiles.toArray();
        if (profilesArray.size() == 1) {
            profile = profilesArray.first();
        } else {
            QStringList profileNames;
            QMap<QString, QJsonValue> profileMap;
            for (const auto& p : profilesArray) {
                QString name = p.toObject().value("name").toString();
                if (!name.isEmpty()) {
                    profileNames.append(name);
                    profileMap.insert(name, p);
                }
            }

            bool ok = false;
            QString chosen = QInputDialog::getItem(nullptr, tr("Select Profile"), tr("Select a profile:"), profileNames, 0, false, &ok);
            if (!ok || !profileMap.contains(chosen)) {
                emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Authentication cancelled."));
                return;
            }
            profile = profileMap[chosen];
            m_didSelectProfile = true;
        }
    }

    auto profileObj = profile.toObject();
    m_data->minecraftProfile.name = profileObj.value("name").toString();
    m_data->minecraftProfile.id = profileObj.value("id").toString();

    if (m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication server didn't send profile UUID."));
        return;
    }

    if (m_didSelectProfile) {
        m_didSelectProfile = false;
        refresh();
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Logged in successfully."));
}

void YggdrasilStep::processError(const QJsonObject& responseData)
{
    QString msg = responseData.value("errorMessage").toString();
    if (msg.isEmpty()) {
        msg = tr("An unknown authentication error occurred.");
    }
    emit finished(AccountTaskState::STATE_FAILED_HARD, msg);
}
