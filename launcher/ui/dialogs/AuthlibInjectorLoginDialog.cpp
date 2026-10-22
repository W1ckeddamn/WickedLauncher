#include "AuthlibInjectorLoginDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_AuthlibInjectorLoginDialog.h"

#include "Application.h"
#include <QtWidgets/QPushButton>

AuthlibInjectorLoginDialog::AuthlibInjectorLoginDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AuthlibInjectorLoginDialog)
{
    ui->setupUi(this);
    ui->userTextBox->setFocus();
    ui->loadingLabel->setVisible(false);
    ui->errorMessage->setVisible(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    setAcceptDrops(true);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AuthlibInjectorLoginDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AuthlibInjectorLoginDialog::~AuthlibInjectorLoginDialog() = default;

QString AuthlibInjectorLoginDialog::fixUrl(const QString& url)
{
    QString fixed{ url.trimmed() };
    if (!fixed.contains("://")) {
        fixed.prepend("https://");
    }
    if (fixed.endsWith("/")) {
        fixed.chop(1);
    }
    return fixed;
}

QUrl AuthlibInjectorLoginDialog::extractAuthlibInjectorUrl(const QMimeData* mimeData)
{
    if (mimeData && mimeData->hasText()) {
        const QString text = mimeData->text();
        const QString prefix = "authlib-injector:yggdrasil-server:";
        if (text.startsWith(prefix)) {
            QString urlStr = text.mid(prefix.length());
            QUrl url = QUrl(QUrl::fromPercentEncoding(urlStr.toUtf8()));
            if (url.isValid()) {
                return url;
            }
        }
    }
    return {};
}

void AuthlibInjectorLoginDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (extractAuthlibInjectorUrl(event->mimeData()).isValid()) {
        event->acceptProposedAction();
    }
}

void AuthlibInjectorLoginDialog::dropEvent(QDropEvent* event)
{
    const auto& url = extractAuthlibInjectorUrl(event->mimeData());
    if (url.isValid()) {
        ui->authlibInjectorTextBox->setText(url.toString());
    }
    event->acceptProposedAction();
}

void AuthlibInjectorLoginDialog::accept()
{
    ui->errorMessage->setVisible(false);
    QString fixedUrl = fixUrl(ui->authlibInjectorTextBox->text());

    auto response = CustomMessageBox::selectable(this, tr("Confirm account creation"),
                                                 tr("Warning: you are about to send your credentials to an "
                                                    "unofficial, third-party authentication server:\n"
                                                    "%1\n\n"
                                                    "Never use your Microsoft password for a third-party account!\n\n"
                                                    "Are you sure you want to proceed?")
                                                     .arg(fixedUrl),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }

    setUserInputsEnabled(false);
    ui->loadingLabel->setVisible(true);

    auto netJob = NetJob::Ptr(new NetJob("Get authlib-injector API root", APPLICATION->network()));
    netJob->setAskRetry(false);

    auto username = ui->userTextBox->text();
    m_apiLocationRequest = GetAuthlibInjectorApiLocation::make(QUrl(fixedUrl), m_account, username);
    netJob->addNetAction(m_apiLocationRequest);

    m_apiLocationTask.reset(netJob);
    connect(netJob.get(), &NetJob::succeeded, this, &AuthlibInjectorLoginDialog::onApiLocationTaskSucceeded);
    connect(netJob.get(), &NetJob::failed, this, &AuthlibInjectorLoginDialog::onApiLocationTaskFailed);
    m_apiLocationTask->start();
}

void AuthlibInjectorLoginDialog::setUserInputsEnabled(bool enable)
{
    ui->userTextBox->setEnabled(enable);
    ui->passTextBox->setEnabled(enable);
    ui->authlibInjectorTextBox->setEnabled(enable);
    ui->buttonBox->setEnabled(enable);
}

void AuthlibInjectorLoginDialog::on_userTextBox_textEdited(const QString&)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!ui->userTextBox->text().isEmpty() && !ui->passTextBox->text().isEmpty() && !ui->authlibInjectorTextBox->text().isEmpty());
}

void AuthlibInjectorLoginDialog::on_passTextBox_textEdited(const QString&)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!ui->userTextBox->text().isEmpty() && !ui->passTextBox->text().isEmpty() && !ui->authlibInjectorTextBox->text().isEmpty());
}

void AuthlibInjectorLoginDialog::on_authlibInjectorTextBox_textEdited(const QString&)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!ui->userTextBox->text().isEmpty() && !ui->passTextBox->text().isEmpty() && !ui->authlibInjectorTextBox->text().isEmpty());
}

void AuthlibInjectorLoginDialog::onApiLocationTaskFailed(const QString& reason)
{
    onTaskFailed(reason);
}

void AuthlibInjectorLoginDialog::onTaskFailed(const QString& reason)
{
    ui->errorMessage->setText(QString("<font color='red'>%1</font>").arg(reason));
    ui->errorMessage->setVisible(true);

    setUserInputsEnabled(true);
    ui->loadingLabel->setVisible(false);
}

void AuthlibInjectorLoginDialog::onApiLocationTaskSucceeded()
{
    m_account = m_apiLocationRequest->getAccount();
    m_loginTask = m_account->login(false, ui->passTextBox->text());
    connect(m_loginTask.get(), &Task::failed, this, &AuthlibInjectorLoginDialog::onTaskFailed);
    connect(m_loginTask.get(), &Task::succeeded, this, &AuthlibInjectorLoginDialog::onTaskSucceeded);
    connect(m_loginTask.get(), &Task::status, this, &AuthlibInjectorLoginDialog::onTaskStatus);
    m_loginTask->start();
}

void AuthlibInjectorLoginDialog::onTaskSucceeded()
{
    QDialog::accept();
}

void AuthlibInjectorLoginDialog::onTaskStatus(const QString& status)
{
    ui->errorMessage->setText(status);
}

MinecraftAccountPtr AuthlibInjectorLoginDialog::newAccount(QWidget* parent, const QString& message)
{
    AuthlibInjectorLoginDialog dlg(parent);
    dlg.ui->label->setText(message);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}
