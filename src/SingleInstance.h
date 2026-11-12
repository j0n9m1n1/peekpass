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
    void setActivationHandler(std::function<void()> handler);
    QString errorString() const;

private:
    QString m_directory;
    QString m_error;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    std::function<void()> m_activate;
    bool m_pendingActivation = false;
};
