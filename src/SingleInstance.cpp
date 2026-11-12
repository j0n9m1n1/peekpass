#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

SingleInstance::SingleInstance(const QString& directory)
    : m_directory(directory.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
          : directory)
{
}

SingleInstance::Result SingleInstance::start()
{
    if (!QDir().mkpath(m_directory)) {
        m_error = QStringLiteral("Could not create the application runtime directory.");
        return Result::Error;
    }
    const QString name = QStringLiteral("peekpass-") + QString::fromLatin1(
        QCryptographicHash::hash(QDir(m_directory).absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex());
    m_lock = std::make_unique<QLockFile>(QDir(m_directory).filePath(QStringLiteral("instance.lock")));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock()) {
        // The first process may still be setting up its listener.
        QString socketError;
        for (int attempt = 0; attempt < 20; ++attempt) {
            QLocalSocket socket;
            socket.connectToServer(name);
            if (socket.waitForConnected(100)) {
                socket.write("show");
                if (socket.bytesToWrite() == 0 || socket.waitForBytesWritten(1000))
                    return Result::ActivatedExisting;
            }
            socketError = socket.errorString();
            QThread::msleep(50);
        }
        m_error = QStringLiteral("peekpass is already running but could not be activated: ") + socketError;
        return Result::Error;
    }

    QLocalServer::removeServer(name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(name)) {
        m_error = m_server.errorString();
        return Result::Error;
    }
    QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
        while (QLocalSocket* socket = m_server.nextPendingConnection()) {
            auto receive = [this, socket] {
                if (socket->bytesAvailable() < 4)
                    return;
                if (socket->read(4) == QByteArrayLiteral("show")) {
                    if (m_activate)
                        m_activate();
                    else
                        m_pendingActivation = true;
                }
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, receive);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            receive();
        }
    });
    return Result::Primary;
}

void SingleInstance::setActivationHandler(std::function<void()> handler)
{
    m_activate = std::move(handler);
    if (m_pendingActivation && m_activate) {
        m_pendingActivation = false;
        m_activate();
    }
}

QString SingleInstance::errorString() const
{
    return m_error;
}
