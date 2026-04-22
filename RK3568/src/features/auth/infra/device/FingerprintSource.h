/**
 * @file FingerprintSource.h
 * @brief 指纹采集源（dlopen libzazapi.so + QTimer 轮询）
 */
#ifndef FINGERPRINT_SOURCE_H
#define FINGERPRINT_SOURCE_H

#include "features/auth/domain/ports/ICredentialSource.h"

#include <QElapsedTimer>

class QTimer;

class FingerprintSource : public ICredentialSource
{
    Q_OBJECT
public:
    explicit FingerprintSource(QObject *parent = nullptr);
    ~FingerprintSource() override;

    AuthMode mode() const override;
    void start() override;
    void stop() override;

    /**
     * @brief 启动注册模式：两次采集 + RegModule 合成
     *
     * 流程：第1次按 → 松手确认 → 第2次按 → 合成 → 上传模板
     */
    void startEnroll();

signals:
    /**
     * @brief 注册采集进度信号
     * @param step 当前步骤
     * @param hint 可供 UI 直接显示的提示文本
     */
    void enrollProgress(int step, const QString &hint);

private slots:
    void poll();

private:
    enum EnrollState {
        Idle = 0,
        WaitPress1,     ///< 等待第一次按指纹
        WaitLift,       ///< 等待用户松手
        WaitPress2,     ///< 等待第二次按指纹
    };

    bool loadSdk();
    bool initDevice();
    void closeDevice();

    void *m_lib;
    int m_handle;
    bool m_deviceOpen;
    QTimer *m_pollTimer;
    qint64 m_lastCaptureMs;

    bool m_enrollMode;
    EnrollState m_enrollState;
    int m_liftConfirmCount;         ///< 连续检测到 NO_FINGER 的次数
    QElapsedTimer m_stepTimer;      ///< 每步超时计时
};

#endif // FINGERPRINT_SOURCE_H
