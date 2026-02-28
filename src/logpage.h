/**
 * @file logpage.h
 * @brief 服务日志页面 - 负责后端服务启动编排与日志展示
 * @author RK3568 项目组
 * @date 2026-02-10
 *
 * 设计说明：
 * 1. 该类遵循单一职责原则：仅负责“服务启动流程编排 + 启动日志输出”。
 * 2. 不直接承担业务功能（如登录、工单处理），避免 UI 页面间耦合。
 * 3. 通过 ConfigManager 读取配置，减少硬编码，提高跨平台可维护性。
 */

#ifndef LOGPAGE_H
#define LOGPAGE_H

#include <QWidget>
#include <QProcess>
#include <QPlainTextEdit>
#include <QTimer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include <QStringList>

namespace Ui
{
    class LogPage;
}

/**
 * @class LogPage
 * @brief 服务日志与启动控制页面
 *
 * 适用场景：
 * 1. 应用启动阶段自动检测并拉起后端服务。
 * 2. 在 Windows 联调环境下，支持 remote 不可达时回退 local 脚本拉起。
 * 3. 在 Linux/RK3588 部署环境下，支持纯 remote 可达性检查（由 systemd 托管后端）。
 */
class LogPage : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 【函数作用】构造服务日志页面并初始化自动启动流程。
     * @param parent：QWidget* - 父窗口指针，可为空（默认 nullptr）。
     * @return 无。
     * @note 注意事项：构造后会启动一次单次定时器，延迟触发自动启动逻辑。
     */
    explicit LogPage(QWidget *parent = nullptr);

    /**
     * @brief 【函数作用】析构页面并安全释放进程/定时器资源。
     * @return 无。
     * @note 注意事项：若脚本进程仍在运行，会先 terminate 再释放。
     */
    ~LogPage();

private slots:
    /**
     * @brief 【函数作用】根据配置执行自动启动主流程（local/remote 分支）。
     * @return 无。
     * @note 注意事项：由单次定时器触发，避免页面构造阶段阻塞 UI。
     */
    void autoStartServices();

    /**
     * @brief 【函数作用】脚本进程启动成功后的异步回调。
     * @return 无。
     */
    void onProcessStarted();

    /**
     * @brief 【函数作用】读取并输出脚本标准输出日志。
     * @return 无。
     */
    void onProcessReadyReadOutput();

    /**
     * @brief 【函数作用】读取并输出脚本标准错误日志。
     * @return 无。
     */
    void onProcessReadyReadError();

    /**
     * @brief 【函数作用】处理脚本进程退出事件。
     * @param exitCode：int - 进程退出码。
     * @param exitStatus：QProcess::ExitStatus - 退出状态（正常/异常）。
     * @return 无。
     */
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief 【函数作用】处理脚本进程启动/运行错误并执行重试策略。
     * @param error：QProcess::ProcessError - 进程错误类型。
     * @return 无。
     */
    void onProcessError(QProcess::ProcessError error);

    /**
     * @brief 【函数作用】重试定时器触发时重新执行启动流程。
     * @return 无。
     */
    void onStartRetryTimeout();

private:
    /**
     * @brief 【函数作用】初始化日志控件并输出启动横幅。
     * @return 无。
     */
    void initServiceLog();

    /**
     * @brief 【函数作用】执行本地脚本启动流程。
     * @return 无。
     * @note 注意事项：内部会进行端口占用判定，必要时直接拒绝启动以避免端口冲突。
     */
    void startServices();

    /**
     * @brief 【函数作用】追加一条日志并滚动到底部。
     * @param text：const QString& - 要输出的日志文本，不允许为空字符串（空字符串会被原样写入）。
     * @return 无。
     */
    void appendLog(const QString &text);

    /**
     * @brief 【函数作用】检测端口是否可连接（是否已被占用）。
     * @param port：int - 端口号，范围 1~65535。
     * @return bool - true 表示端口已被占用，false 表示未占用或不可达。
     */
    bool checkPortInUse(int port) const;

    /**
     * @brief 【函数作用】按端口查找并终止占用该端口的进程。
     * @param port：int - 端口号，范围 1~65535。
     * @return 无。
     * @note 注意事项：此操作可能终止非本程序进程，仅在显式启用 autoCleanPorts 时调用。
     */
    void killProcessByPort(int port);

    /**
     * @brief 【函数作用】检查并按配置处理端口占用问题。
     * @return bool - true 表示允许继续启动脚本；false 表示应中止本次启动。
     * @note 注意事项：当检测到占用且未启用自动清理时，返回 false，避免重复拉起导致冲突。
     */
    bool checkAndCleanPorts();

    /**
     * @brief 【函数作用】解析服务工作目录（配置优先，默认兜底）。
     * @return QString - 有效目录路径；为空表示无有效目录。
     */
    QString resolveServiceWorkDir() const;

    /**
     * @brief 【函数作用】解析启动脚本路径（支持绝对/相对路径与平台差异）。
     * @return QString - 有效脚本绝对路径；为空表示未找到脚本。
     */
    QString resolveServiceScriptPath() const;

    /**
     * @brief 【函数作用】解析服务端口集合（用于端口检测与清理）。
     * @return QList<int> - 端口列表。
     */
    QList<int> resolveServicePorts() const;

    /**
     * @brief 【函数作用】解析 remote 模式严格检查的关键端口集合。
     * @return QList<int> - 关键端口列表（默认 8081,9000）。
     */
    QList<int> resolveRemoteRequiredPorts() const;

    /**
     * @brief 【函数作用】读取是否启用端口自动清理配置。
     * @return bool - true 启用自动清理；false 禁用。
     */
    bool isAutoCleanPortsEnabled() const;

    /**
     * @brief 【函数作用】按平台方式启动脚本进程。
     * @param scriptPath：const QString& - 脚本绝对路径，不允许为空。
     * @return bool - true 表示启动命令已发出；false 表示无法启动。
     */
    bool startScriptProcess(const QString &scriptPath);

    /**
     * @brief 【函数作用】读取并规范化服务启动模式配置。
     * @return QString - "local" 或 "remote"。
     */
    QString serviceStartMode() const;

    /**
     * @brief 【函数作用】执行 remote 模式健康检查（URL+关键端口）。
     * @param detail：QString* - 可选输出参数，返回失败/成功细节；允许为空。
     * @return bool - true 表示服务可达；false 表示不可达。
     */
    bool checkRemoteServiceReady(QString *detail) const;

    /**
     * @brief 【函数作用】启动外部日志文件实时跟踪（tail）。
     * @return 无。
     * @note 注意事项：仅在未由当前进程直接拉起后端时使用，用于观测“已在运行”服务日志。
     */
    void startExternalLogTailing();

    /**
     * @brief 【函数作用】停止外部日志文件实时跟踪。
     * @return 无。
     */
    void stopExternalLogTailing();

    /**
     * @brief 【函数作用】轮询读取外部日志文件新增内容。
     * @return 无。
     */
    void pollExternalLogFiles();

    /**
     * @brief 【函数作用】解析需要跟踪的外部日志文件列表。
     * @return QStringList - 日志文件绝对路径列表。
     */
    QStringList resolveExternalLogFiles() const;

    /**
     * @brief 【函数作用】根据失败原因调度下一次启动重试。
     * @param reason：const QString& - 失败原因描述。
     * @return 无。
     */
    void scheduleStartRetry(const QString &reason);

    Ui::LogPage *ui;
    QProcess *m_process;          // 启动脚本的进程
    QPlainTextEdit *m_logDisplay; // 日志显示区域
    QTimer *m_autoStartTimer;     // 自动启动定时器
    QTimer *m_startRetryTimer;    // 启动失败重试定时器（避免阻塞 UI）
    QTimer *m_externalLogTimer;   // 外部日志轮询定时器
    QStringList m_externalLogFiles; // 当前跟踪的外部日志文件列表
    QHash<QString, qint64> m_externalLogOffsets; // 每个日志文件的读取偏移量
    int m_startAttempt;           // 当前启动尝试次数
    int m_maxStartRetries;        // 最大重试次数
    bool m_startInProgress;       // 是否处于启动流程中（防抖）
    bool m_externalLogTailingStarted; // 是否已启动外部日志跟踪
};

#endif // LOGPAGE_H
