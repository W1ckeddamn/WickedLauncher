#pragma once

#include <QDialog>
#include <QMimeData>
#include <QUrl>
#include <memory>

#include "GetAuthlibInjectorApiLocation.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "tasks/Task.h"

namespace Ui {
class AuthlibInjectorLoginDialog;
}

class AuthlibInjectorLoginDialog : public QDialog {
    Q_OBJECT

   public:
    ~AuthlibInjectorLoginDialog();

    static MinecraftAccountPtr newAccount(QWidget* parent, const QString& message);

   private:
    explicit AuthlibInjectorLoginDialog(QWidget* parent = nullptr);

    void setUserInputsEnabled(bool enable);

   protected slots:
    void accept() override;

    void onApiLocationTaskSucceeded();
    void onApiLocationTaskFailed(const QString& reason);
    void onTaskFailed(const QString& reason);
    void onTaskSucceeded();
    void onTaskStatus(const QString& status);

    void on_userTextBox_textEdited(const QString& newText);
    void on_passTextBox_textEdited(const QString& newText);
    void on_authlibInjectorTextBox_textEdited(const QString& newText);

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

   private:
    QString fixUrl(const QString& url);
    QUrl extractAuthlibInjectorUrl(const QMimeData* mimeData);

    std::unique_ptr<Ui::AuthlibInjectorLoginDialog> ui;
    MinecraftAccountPtr m_account;
    Task::Ptr m_loginTask;
    Task::Ptr m_apiLocationTask;
    GetAuthlibInjectorApiLocation::Ptr m_apiLocationRequest;
};
