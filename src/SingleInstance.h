#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <functional>
#include <memory>

class SingleInstance
{
public:
    enum class Result { Primary, ActivatedExisting, Error };
    explicit SingleInstance(const QString& directory = {});
    Result start();
    bool requestQuit(int waitExitMs = 2500);
    void setActivationHandler(std::function<void()> handler);
    void setQuitHandler(std::function<void()> handler);
    QString errorString() const;

private:
    QString serverName() const;
    QString m_directory;
    QString m_error;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    std::function<void()> m_activate;
    std::function<void()> m_quit;
    bool m_pendingActivation = false;
    bool m_pendingQuit = false;
};
