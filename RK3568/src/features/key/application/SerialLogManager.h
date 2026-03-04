/**
 * @file SerialLogManager.h
 * @brief 串口结构化日志数据管理器（无 UI 依赖）
 *
 * 设计目标：
 * 1. 把日志的存储/过滤/导出从 KeyManagePage 中抽离，降低页面类复杂度。
 * 2. 保持行为不变：专家模式过滤、导出 CSV、日志上限自动淘汰旧数据。
 * 3. 只做数据处理，不直接操作任何 QWidget。
 */
#ifndef SERIALLOGMANAGER_H
#define SERIALLOGMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include "features/key/protocol/LogItem.h"

class SerialLogManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialLogManager(QObject *parent = nullptr);

    void append(const LogItem &item);
    void clear();

    void setExpertMode(bool on);
    void setShowHex(bool on);
    bool expertMode() const { return m_expertMode; }
    bool showHex() const { return m_showHex; }

    const QList<LogItem> &items() const { return m_items; }
    QList<LogItem> filteredItems() const;
    bool shouldDisplay(const LogItem &item) const;

    bool exportCsv(const QString &path, QString *error = nullptr) const;

signals:
    void overflowDropped();

private:
    static const int kMaxItems = 5000;

    static QString dirText(LogDir dir);
    static QString cmdText(quint8 cmd);

    QList<LogItem> m_items;
    bool m_expertMode;
    bool m_showHex;
    bool m_overflowNotified;
};

#endif // SERIALLOGMANAGER_H
