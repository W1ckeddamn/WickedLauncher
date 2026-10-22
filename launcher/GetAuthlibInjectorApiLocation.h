#pragma once

#include "minecraft/auth/MinecraftAccount.h"
#include "net/Request.h"

class GetAuthlibInjectorApiLocation : public Net::Request {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<GetAuthlibInjectorApiLocation>;
    GetAuthlibInjectorApiLocation(const QUrl& url, MinecraftAccountPtr account, const QString& username);
    ~GetAuthlibInjectorApiLocation() override = default;

    static GetAuthlibInjectorApiLocation::Ptr make(const QUrl& url, MinecraftAccountPtr account, const QString& username);

    MinecraftAccountPtr getAccount() const;

    class Sink : public Net::Sink {
       public:
        explicit Sink(GetAuthlibInjectorApiLocation& outer) : m_outer(outer) {}
        ~Sink() override = default;

        InitResult init(QNetworkRequest& request) override;
        Result<> write(const QByteArray& data) override;
        void abort() override;
        Result<> finalize(QNetworkReply& reply) override;
        bool hasLocalData() override { return false; }

       private:
        GetAuthlibInjectorApiLocation& m_outer;
    };

   private:
    MinecraftAccountPtr m_account;
    QString m_username;
};
