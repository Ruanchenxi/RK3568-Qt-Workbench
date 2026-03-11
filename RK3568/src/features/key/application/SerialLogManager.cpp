/**
 * @file SerialLogManager.cpp
 * @brief 串口结构化日志数据管理器实现
 */
#include "SerialLogManager.h"
#include "features/key/protocol/KeyProtocolDefs.h"

#include <QFile>
#include <QTextStream>

SerialLogManager::SerialLogManager(QObject *parent)
    : QObject(parent)
    , m_expertMode(false)
    , m_showHex(true)
    , m_overflowNotified(false)
{
}

void SerialLogManager::append(const LogItem &item)
{
    if (m_items.size() >= kMaxItems) {
        m_items.removeFirst();
        if (!m_overflowNotified) {
            m_overflowNotified = true;
            emit overflowDropped();
        }
    }
    m_items.append(item);
}

void SerialLogManager::clear()
{
    m_items.clear();
    m_overflowNotified = false;
}

void SerialLogManager::setExpertMode(bool on)
{
    m_expertMode = on;
}

void SerialLogManager::setShowHex(bool on)
{
    m_showHex = on;
}

bool SerialLogManager::shouldDisplay(const LogItem &item) const
{
    if (item.expertOnly && !m_expertMode) {
        return false;
    }
    return true;
}

QList<LogItem> SerialLogManager::filteredItems() const
{
    QList<LogItem> list;
    list.reserve(m_items.size());
    for (const LogItem &item : m_items) {
        if (shouldDisplay(item)) {
            list.append(item);
        }
    }
    return list;
}

bool SerialLogManager::exportCsv(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QObject::tr("无法写入文件: %1").arg(path);
        }
        return false;
    }

    // Excel 兼容：写 UTF-8 BOM，避免中文列头乱码。
    file.write("\xEF\xBB\xBF");

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QStringLiteral("时间,方向,OpId,命令,长度,摘要,Hex\n");

    for (const LogItem &item : m_items) {
        QString summaryEscaped = item.summary;
        summaryEscaped.replace("\"", "\"\"");

        const QString hexText = item.hex.isEmpty()
            ? QStringLiteral("--")
            : QString(item.hex.toHex(' ').toUpper());

        out << item.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << ","
            << dirText(item.dir) << ","
            << item.opId << ","
            << cmdText(item.cmd) << ","
            << item.length << ",\""
            << summaryEscaped << "\",\""
            << hexText << "\"\n";
    }

    if (error) {
        error->clear();
    }
    return true;
}

QString SerialLogManager::dirText(LogDir dir)
{
    switch (dir) {
    case LogDir::TX:    return QStringLiteral("TX>");
    case LogDir::RX:    return QStringLiteral("RX<");
    case LogDir::RAW:   return QStringLiteral("RAW~");
    case LogDir::EVENT: return QStringLiteral("EVT*");
    case LogDir::WARN:  return QStringLiteral("WARN!");
    case LogDir::ERROR: return QStringLiteral("ERR!");
    default:            return QStringLiteral("?");
    }
}

QString SerialLogManager::cmdText(quint8 cmd)
{
    switch (cmd) {
    case LogCmdNone:                return QStringLiteral("--");
    case KeyProtocol::CmdInit:     return QStringLiteral("INIT");
    case static_cast<quint8>(KeyProtocol::CmdInit | 0x80): return QStringLiteral("INIT_MORE");
    case KeyProtocol::CmdSetCom:   return QStringLiteral("SET_COM");
    case KeyProtocol::CmdQTask:    return QStringLiteral("Q_TASK");
    case KeyProtocol::CmdITaskLog: return QStringLiteral("I_TASK_LOG");
    case KeyProtocol::CmdDel:      return QStringLiteral("DEL");
    case KeyProtocol::CmdDownloadRfid:return QStringLiteral("DN_RFID");
    case static_cast<quint8>(KeyProtocol::CmdDownloadRfid | 0x80): return QStringLiteral("DN_RFID_MORE");
    case KeyProtocol::CmdTicket:   return QStringLiteral("TICKET");
    case KeyProtocol::CmdTicketMore:return QStringLiteral("TICKET_MORE");
    case KeyProtocol::CmdUpTaskLog:return QStringLiteral("UP_TASK_LOG");
    case KeyProtocol::CmdAck:      return QStringLiteral("ACK");
    case KeyProtocol::CmdNak:      return QStringLiteral("NAK");
    case KeyProtocol::CmdKeyEvent: return QStringLiteral("KEY_EVT");
    default:
        return QStringLiteral("0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
    }
}
