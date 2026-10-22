#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/NetRequest.h"

class AuthlibInjectorMetadataStep : public AuthStep {
    Q_OBJECT

   public:
    explicit AuthlibInjectorMetadataStep(AccountData* data);
    virtual ~AuthlibInjectorMetadataStep() noexcept = default;

    void perform() override;

    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};
