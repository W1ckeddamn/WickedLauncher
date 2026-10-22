#pragma once
#include <QObject>
#include <optional>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/NetRequest.h"

class YggdrasilStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilStep(AccountData* data, const std::optional<QString>& password);
    virtual ~YggdrasilStep() noexcept = default;

    void perform() override;

    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    void login(const QString& password);
    void refresh();

    void processResponse(const QJsonObject& responseData);
    void processError(const QJsonObject& responseData);

    std::optional<QString> m_password;
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
    bool m_didSelectProfile = false;
};
