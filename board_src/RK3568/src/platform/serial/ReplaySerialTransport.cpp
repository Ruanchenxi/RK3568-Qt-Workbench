/**
 * @file ReplaySerialTransport.cpp
 * @brief 串口回放传输实现（无硬件验证用）
 */
#include "platform/serial/ReplaySerialTransport.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QtGlobal>

ReplaySerialTransport::ReplaySerialTransport(QObject *parent)
    : ISerialTransport(parent)
    , m_opened(false)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ReplaySerialTransport::runNextAction);
}

void ReplaySerialTransport::setPortName(const QString &name)
{
    m_portName = name;
}

QString ReplaySerialTransport::portName() const
{
    return m_portName;
}

bool ReplaySerialTransport::open(int)
{
    m_opened = true;
    emit openedChanged(true);
    m_timer.start(0);
    return true;
}

void ReplaySerialTransport::close()
{
    m_timer.stop();
    m_opened = false;
    emit openedChanged(false);
}

bool ReplaySerialTransport::isOpen() const
{
    return m_opened;
}

qint64 ReplaySerialTransport::write(const QByteArray &data)
{
    // 回放模式下仅记录写入长度，真实回包由脚本驱动。
    return data.size();
}

QByteArray ReplaySerialTransport::readAll()
{
    const QByteArray out = m_readBuffer;
    m_readBuffer.clear();
    return out;
}

qint64 ReplaySerialTransport::bytesAvailable() const
{
    return m_readBuffer.size();
}

bool ReplaySerialTransport::flush()
{
    return true;
}

bool ReplaySerialTransport::waitForBytesWritten(int)
{
    return true;
}

QString ReplaySerialTransport::errorString() const
{
    return QString();
}

bool ReplaySerialTransport::setDataTerminalReady(bool)
{
    return true;
}

bool ReplaySerialTransport::setRequestToSend(bool)
{
    return true;
}

bool ReplaySerialTransport::isDataTerminalReady()
{
    return true;
}

bool ReplaySerialTransport::isRequestToSend()
{
    return true;
}

bool ReplaySerialTransport::loadScript(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法打开脚本: %1").arg(path);
        }
        return false;
    }

    QQueue<QVariantMap> newActions;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) {
                *error = QStringLiteral("脚本解析失败: %1").arg(parseError.errorString());
            }
            return false;
        }
        newActions.enqueue(doc.object().toVariantMap());
    }

    m_actions = newActions;
    if (error) {
        error->clear();
    }
    return true;
}

void ReplaySerialTransport::clearScript()
{
    m_actions.clear();
}

void ReplaySerialTransport::runNextAction()
{
    if (!m_opened || m_actions.isEmpty()) {
        return;
    }

    const QVariantMap action = m_actions.dequeue();
    const QString op = action.value("op").toString().trimmed().toLower();
    const int delayMs = action.value("delayMs").toInt();

    if (op == "open") {
        emit openedChanged(true);
    } else if (op == "chunk") {
        const QByteArray bytes = parseHex(action.value("hex").toString());
        if (!bytes.isEmpty()) {
            m_readBuffer.append(bytes);
            emit readyRead();
        }
    } else if (op == "error") {
        const int code = action.value("code").toInt();
        const QString text = action.value("text").toString();
        emit errorOccurred(code, text);
    } else if (op == "timeout") {
        // timeout 仅通过等待时间表达，不主动发数据。
    }

    if (!m_actions.isEmpty()) {
        m_timer.start(qMax(0, delayMs));
    }
}

QByteArray ReplaySerialTransport::parseHex(const QString &hex) const
{
    QString normalized = hex;
    normalized.remove(' ');
    normalized.remove('\t');
    normalized.remove('\r');
    normalized.remove('\n');
    return QByteArray::fromHex(normalized.toLatin1());
}
