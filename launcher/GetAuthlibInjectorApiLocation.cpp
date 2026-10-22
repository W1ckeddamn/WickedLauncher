#include "GetAuthlibInjectorApiLocation.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"

GetAuthlibInjectorApiLocation::GetAuthlibInjectorApiLocation(const QUrl& url, MinecraftAccountPtr account, const QString& username)
    : Net::Request(url), m_account(account), m_username(username)
{
    m_sink.reset(new Sink(*this));
}

GetAuthlibInjectorApiLocation::Ptr GetAuthlibInjectorApiLocation::make(const QUrl& url, MinecraftAccountPtr account, const QString& username)
{
    return GetAuthlibInjectorApiLocation::Ptr(new GetAuthlibInjectorApiLocation(url, account, username));
}

Net::Sink::InitResult GetAuthlibInjectorApiLocation::Sink::init(QNetworkRequest&)
{
    return InitType::Ok;
}

Result<> GetAuthlibInjectorApiLocation::Sink::write(const QByteArray&)
{
    return {};
}

void GetAuthlibInjectorApiLocation::Sink::abort()
{
}

Result<> GetAuthlibInjectorApiLocation::Sink::finalize(QNetworkReply& reply)
{
    QByteArray header = reply.rawHeader("X-Authlib-Injector-API-Location");
    QUrl url = m_outer.url();
    if (!header.isEmpty()) {
        url = url.resolved(QUrl(QString::fromUtf8(header)));
    } else {
        qDebug() << "X-Authlib-Injector-API-Location header not found, using original url";
    }

    const auto& encodedUrl = QString::fromUtf8(url.toEncoded(QUrl::FullyEncoded));
    m_outer.m_account = MinecraftAccount::createFromUsernameAuthlibInjector(m_outer.m_username, encodedUrl);
    return {};
}

MinecraftAccountPtr GetAuthlibInjectorApiLocation::getAccount() const
{
    return m_account;
}
