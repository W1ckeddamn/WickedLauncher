#pragma once

#include "minecraft/auth/MinecraftAccount.h"
#include "net/NetRequest.h"

class GetAuthlibInjectorApiLocation : public Net::Request {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<GetAuthlibInjectorApiLocation>;
    GetAuthlibInjectorApiLocation(const QUrl& url, MinecraftAccountPtr account, const QString& username);
    virtual ~GetAuthlibInjectorApiLocation() = default;

    static GetAuthlibInjectorApiLocation::Ptr make(const QUrl& url, MinecraftAccountPtr account, const QString& username);

    MinecraftAccountPtr getAccount();

    class Sink : public Net::Sink {
       public:
        Sink(GetAuthlibInjectorApiLocation& outer) : m_outer(outer) {}
        virtual ~Sink() = default;

        auto init(QNetworkRequest& request) -> Task::State override;
        auto write(QByteArray& data) -> Task::State override;
        auto abort() -> Task::State override;
        auto finalize(QNetworkReply& reply) -> Task::State override;

       private:
        GetAuthlibInjectorApiLocation& m_outer;
    };

   private:
    MinecraftAccountPtr m_account;
    QString m_username;
};
