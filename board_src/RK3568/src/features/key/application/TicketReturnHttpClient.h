/**
 * @file TicketReturnHttpClient.h
 * @brief 钥匙回传 HTTP 客户端
 *
 * Layer:
 *   features/key/application
 *
 * Responsibilities:
 *   1) 将已解析的钥匙操作日志组装为 HTTP JSON 请求
 *   2) 向可配置的回传接口发送 POST
 *   3) 输出与原产品风格接近的 HTTP 客户端日志
 *
 * Dependencies:
 *   - Allowed: ConfigManager, QNetworkAccessManager
 *   - Forbidden: UI page classes, protocol parsing logic
 *
 * Constraints:
 *   - 接口地址未知时不做猜测，直接按未配置处理
 *   - 只负责上传，不决定何时发起回传请求
 */
#ifndef TICKETRETURNHTTPCLIENT_H
#define TICKETRETURNHTTPCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class QNetworkAccessManager;

class TicketReturnHttpClient : public QObject
{
    Q_OBJECT
public:
    explicit TicketReturnHttpClient(QObject *parent = nullptr);
    ~TicketReturnHttpClient() override = default;

    bool isConfigured() const;
    QString configuredUrl() const;
    void uploadReturnPayload(const QJsonObject &payload);

signals:
    void logAppended(const QString &text);
    void uploadSucceeded(const QString &taskId);
    void uploadFailed(const QString &taskId, const QString &reason);

private:
    QString nowText() const;
    QString prettyJson(const QByteArray &body) const;

    QNetworkAccessManager *m_network;
};

#endif // TICKETRETURNHTTPCLIENT_H
