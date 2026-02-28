/**
 * @file logpage.cpp
 * @brief 服务日志页面 - 显示服务启动日志
 */

#include "logpage.h"
#include "ui_logpage.h"
#include "core/ConfigManager.h"
#include <QDateTime>
#include <QTextCodec>
#include <QSet>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

LogPage::LogPage(QWidget *parent) : QWidget(parent),
                                    ui(new Ui::LogPage),
                                    m_process(nullptr),
                                    m_logDisplay(nullptr),
                                    m_autoStartTimer(nullptr),
                                    m_startRetryTimer(nullptr),
                                    m_externalLogTimer(nullptr),
                                    m_externalLogFiles(),
                                    m_externalLogOffsets(),
                                    m_startAttempt(0),
                                    m_maxStartRetries(3),
                                    m_startInProgress(false),
                                    m_externalLogTailingStarted(false)
{
    ui->setupUi(this);

    // 初始化日志显示
    initServiceLog();

    // 创建定时器，延迟1秒后自动启动服务
    m_autoStartTimer = new QTimer(this);
    m_autoStartTimer->setSingleShot(true);
    m_autoStartTimer->setInterval(1000); // 1秒延迟
    connect(m_autoStartTimer, &QTimer::timeout, this, &LogPage::autoStartServices);

    // 启动失败重试定时器：仅负责“延迟后再尝试”，不做阻塞等待。
    m_startRetryTimer = new QTimer(this);
    m_startRetryTimer->setSingleShot(true);
    m_startRetryTimer->setInterval(2000); // 默认2秒后重试
    connect(m_startRetryTimer, &QTimer::timeout, this, &LogPage::onStartRetryTimeout);

    // 外部日志轮询定时器：用于“后端已在运行但非本进程拉起”场景下的日志观测。
    m_externalLogTimer = new QTimer(this);
    m_externalLogTimer->setSingleShot(false);
    m_externalLogTimer->setInterval(1000);
    connect(m_externalLogTimer, &QTimer::timeout, this, &LogPage::pollExternalLogFiles);

    // 启动定时器
    m_autoStartTimer->start();
}

LogPage::~LogPage()
{
    stopExternalLogTailing();

    if (m_startRetryTimer)
    {
        m_startRetryTimer->stop();
    }

    if (m_process)
    {
        if (m_process->state() == QProcess::Running)
        {
            m_process->terminate();
            m_process->waitForFinished(3000);
        }
        delete m_process;
    }
    delete ui;
}

void LogPage::initServiceLog()
{
    // 创建日志显示区域（使用 QPlainTextEdit 模拟终端）
    m_logDisplay = new QPlainTextEdit(this);
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setMaximumBlockCount(5000); // 限制最多5000行

    // 设置终端风格样式：黑底绿字
    m_logDisplay->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1E1E1E;" // 深灰黑色背景
        "  color: #00FF00;"            // 绿色文字（类似终端）
        "  border: 1px solid #333333;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  line-height: 1.4;"
        "}"
        "QPlainTextEdit QScrollBar:vertical {"
        "  background-color: #2A2A2A;"
        "  width: 10px;"
        "}"
        "QPlainTextEdit QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  border-radius: 5px;"
        "}"
        "QPlainTextEdit QScrollBar::handle:vertical:hover {"
        "  background-color: #777777;"
        "}");

    // 将日志显示区域添加到 UI
    // 假设 UI 中有一个名为 layoutLog 的布局容器
    if (ui->layoutLog)
    {
        ui->layoutLog->addWidget(m_logDisplay);
    }

    appendLog("=== 服务日志 ===");
    appendLog("等待启动服务...");
}

void LogPage::autoStartServices()
{
    if (!ConfigManager::instance()->autoStart())
    {
        appendLog(QString("[%1] 已关闭自动启动服务（advanced/autoStart=false）")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
        return;
    }

    const QString mode = serviceStartMode();
    appendLog(QString("\n[%1] 准备自动启动服务...").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    appendLog(QString("[INFO] 启动模式: %1").arg(mode));

    if (mode == "remote")
    {
        appendLog("[INFO] remote 模式：不拉起本地脚本，依赖外部服务（建议 systemd）");
        QString detail;
        if (checkRemoteServiceReady(&detail))
        {
            appendLog("[SUCCESS] 外部服务可达，系统可继续运行");
            startExternalLogTailing();
            return;
        }

        appendLog(QString("[WARN] 外部服务暂不可达：%1").arg(detail));

        // 可选回退策略：
        // 在 Windows 联调场景下，当 remote 不可达时自动回退到本地脚本拉起服务，
        // 以避免必须手工双击 start-all.bat。
        const bool fallbackToLocal = ConfigManager::instance()->value("service/remoteFallbackLocal", false).toBool();
        if (fallbackToLocal)
        {
            appendLog("[INFO] 已启用 remote 回退策略，尝试本地脚本启动服务...");
            startServices();
        }
        else
        {
            appendLog("[INFO] 未启用回退策略，本次仅进行可达性检查");
        }
        return;
    }

    startServices();
}

void LogPage::startServices()
{
    // 防止重复点击或多次触发导致并发启动。
    if (m_startInProgress)
    {
        appendLog("[INFO] 启动流程进行中，忽略重复启动请求");
        return;
    }

    // 如果进程已存在且正在运行，先终止
    if (m_process && m_process->state() == QProcess::Running)
    {
        appendLog("[WARN] 服务进程已在运行中，跳过启动");
        return;
    }

    // 创建进程对象
    if (!m_process)
    {
        m_process = new QProcess(this);

        // 连接信号
        connect(m_process, &QProcess::started, this, &LogPage::onProcessStarted);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &LogPage::onProcessReadyReadOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &LogPage::onProcessReadyReadError);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &LogPage::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &LogPage::onProcessError);
    }

    // 启动前检查端口占用（是否清理由配置项控制）。
    // 若端口状态不满足启动条件，直接中止本次脚本拉起，避免重复启动导致冲突。
    appendLog("[INFO] 检查端口占用情况...");
    if (!checkAndCleanPorts())
    {
        QString detail;
        if (checkRemoteServiceReady(&detail))
        {
            appendLog(QString("[SUCCESS] 检测到现有服务已可用，跳过本次脚本启动：%1").arg(detail));
            startExternalLogTailing();
        }
        else
        {
            appendLog(QString("[ERROR] 端口占用且服务未就绪，已取消启动以避免冲突：%1").arg(detail));
        }
        return;
    }

    const QString workDir = resolveServiceWorkDir();
    if (workDir.isEmpty())
    {
        appendLog("[ERROR] 服务工作目录无效，请检查配置 service/workDir");
        return;
    }

    const QString scriptPath = resolveServiceScriptPath();
    if (scriptPath.isEmpty())
    {
        const QString configuredScript = ConfigManager::instance()->value("service/scriptPath").toString();
        appendLog(QString("[ERROR] 未找到服务启动脚本，请检查 service/scriptPath（当前=%1）").arg(configuredScript));
        appendLog(QString("[ERROR] 当前解析到的 service/workDir=%1").arg(workDir));
        return;
    }

    // 设置工作目录
    m_process->setWorkingDirectory(workDir);

    // 设置读取通道
    m_process->setReadChannel(QProcess::StandardOutput);

    // 设置进程环境
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    env.insert("CHCP", "65001"); // Windows 控制台 UTF-8 代码页
#endif
    m_process->setProcessEnvironment(env);

    appendLog(QString("[INFO] 启动脚本: %1").arg(scriptPath));
    appendLog(QString("[INFO] 工作目录: %1").arg(workDir));
    appendLog(QString("[INFO] 执行命令: %1\n").arg(scriptPath));

    // 记录尝试次数，便于重试策略和日志追踪。
    stopExternalLogTailing();
    ++m_startAttempt;
    m_startInProgress = true;
    appendLog(QString("[INFO] 第 %1/%2 次启动尝试")
                  .arg(m_startAttempt)
                  .arg(m_maxStartRetries));

    if (!startScriptProcess(scriptPath))
    {
        m_startInProgress = false;
        appendLog("[ERROR] 启动脚本失败：进程无法启动");
        scheduleStartRetry("QProcess::start 调用失败");
        return;
    }

    // 不在 UI 线程阻塞等待启动，改为异步回调（onProcessStarted/onProcessError）。
    appendLog("[INFO] 启动命令已提交，等待进程回调...\n");
}

void LogPage::onProcessStarted()
{
    // 进程真正启动后，重置重试状态。
    m_startInProgress = false;
    m_startAttempt = 0;
    appendLog("[SUCCESS] 服务脚本已启动，正在执行...\n");
}

void LogPage::onStartRetryTimeout()
{
    appendLog("[INFO] 开始执行延迟重试...");
    startServices();
}

void LogPage::onProcessReadyReadOutput()
{
    if (!m_process)
        return;

    // 读取标准输出（使用 UTF-8 编码解析中文）
    QByteArray data = m_process->readAllStandardOutput();

    // 使用 UTF-8 编码转换
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (!codec)
        codec = QTextCodec::codecForName("UTF8"); // 备用编码

    QString output = codec ? codec->toUnicode(data) : QString::fromUtf8(data);

    // 分行显示，保持原始格式
    QStringList lines = output.split("\n", QString::SkipEmptyParts);
    for (const QString &line : lines)
    {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            appendLog(trimmed);
        }
    }
}

void LogPage::onProcessReadyReadError()
{
    if (!m_process)
        return;

    // 读取错误输出（使用 UTF-8 编码解析中文）
    QByteArray data = m_process->readAllStandardError();

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (!codec)
        codec = QTextCodec::codecForName("GB18030");

    QString error = codec ? codec->toUnicode(data) : QString::fromLocal8Bit(data);

    QStringList lines = error.split("\n", QString::SkipEmptyParts);
    for (const QString &line : lines)
    {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.contains("Active code page"))
        {
            appendLog(QString("[STDERR] %1").arg(trimmed));
        }
    }
}

void LogPage::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_startInProgress = false;

    QString statusText = (exitStatus == QProcess::NormalExit) ? "正常退出" : "异常终止";

    appendLog(QString("\n[%1] 服务脚本执行完成").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    appendLog(QString("[INFO] 退出状态: %1").arg(statusText));
    appendLog(QString("[INFO] 退出代码: %1").arg(exitCode));

    if (exitCode == 0)
    {
        appendLog("[SUCCESS] 所有服务启动成功！");
    }
    else
    {
        appendLog(QString("[WARN] 脚本返回非零退出码: %1").arg(exitCode));
    }

    // start-all 脚本可能很快退出，但后端服务仍在运行。
    // 脚本结束后启用外部日志跟踪，便于继续观测后端日志。
    startExternalLogTailing();
}

void LogPage::onProcessError(QProcess::ProcessError error)
{
    m_startInProgress = false;

    QString errorMsg;
    switch (error)
    {
    case QProcess::FailedToStart:
        errorMsg = "进程启动失败（脚本文件不存在或无执行权限）";
        break;
    case QProcess::Crashed:
        errorMsg = "进程崩溃";
        break;
    case QProcess::Timedout:
        errorMsg = "进程超时";
        break;
    case QProcess::ReadError:
        errorMsg = "读取错误";
        break;
    case QProcess::WriteError:
        errorMsg = "写入错误";
        break;
    case QProcess::UnknownError:
    default:
        errorMsg = "未知错误";
        break;
    }

    appendLog(QString("[ERROR] 进程错误: %1").arg(errorMsg));

    // 错误处理策略：
    // 1) 记录详细日志，便于运维定位；
    // 2) 对可恢复故障执行有限次重试；
    // 3) 不弹阻塞式对话框，避免卡住 UI 线程。
    if (error == QProcess::FailedToStart || error == QProcess::Crashed)
    {
        scheduleStartRetry(errorMsg);
    }
}

void LogPage::appendLog(const QString &text)
{
    if (!m_logDisplay)
        return;

    // 添加日志到显示区域
    m_logDisplay->appendPlainText(text);

    // 自动滚动到底部
    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
}

bool LogPage::checkPortInUse(int port) const
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", port);
    bool connected = socket.waitForConnected(100);
    socket.disconnectFromHost();
    return connected;
}

void LogPage::killProcessByPort(int port)
{
    appendLog(QString("[INFO] 正在清理端口 %1...").arg(port));

#ifdef Q_OS_WIN
    // Windows：使用 netstat + taskkill
    QProcess findProcess;
    findProcess.start("cmd.exe", QStringList() << "/c" << QString("netstat -ano | findstr :%1").arg(port));
    findProcess.waitForFinished(3000);

    QString output = QString::fromLocal8Bit(findProcess.readAllStandardOutput());
    QStringList lines = output.split("\n", QString::SkipEmptyParts);

    QSet<QString> pidsToKill;
    for (const QString &line : lines)
    {
        QStringList parts = line.simplified().split(" ");
        if (parts.size() >= 5)
        {
            const QString pid = parts.last().trimmed();
            if (!pid.isEmpty() && pid != "0")
            {
                pidsToKill.insert(pid);
            }
        }
    }

    for (const QString &pid : pidsToKill)
    {
        appendLog(QString("[INFO] 正在终止进程 PID=%1...").arg(pid));
        QProcess killProcess;
        killProcess.start("cmd.exe", QStringList() << "/c" << QString("taskkill /F /PID %1").arg(pid));
        killProcess.waitForFinished(2000);

        const QString result = QString::fromLocal8Bit(killProcess.readAllStandardOutput());
        if (result.contains("SUCCESS") || result.contains("成功"))
        {
            appendLog(QString("[SUCCESS] 已终止进程 PID=%1").arg(pid));
        }
    }
#else
    // Linux：优先使用 fuser 清理端口；若命令不存在，记录错误并跳过
    QProcess killProcess;
    killProcess.start("/bin/sh", QStringList() << "-c" << QString("fuser -k %1/tcp").arg(port));
    killProcess.waitForFinished(3000);
    if (killProcess.exitStatus() == QProcess::NormalExit && killProcess.exitCode() == 0)
    {
        appendLog(QString("[SUCCESS] 已尝试清理端口 %1").arg(port));
    }
    else
    {
        appendLog(QString("[WARN] 端口 %1 清理失败或未找到 fuser，保留原状态").arg(port));
    }
#endif
}

bool LogPage::checkAndCleanPorts()
{
    const QList<int> ports = resolveServicePorts();
    const bool allowAutoClean = isAutoCleanPortsEnabled();

    bool anyPortInUse = false;
    for (int port : ports)
    {
        if (checkPortInUse(port))
        {
            appendLog(QString("[WARN] 端口 %1 已被占用").arg(port));
            anyPortInUse = true;
        }
    }

    if (anyPortInUse)
    {
        if (!allowAutoClean)
        {
            appendLog("[WARN] 检测到端口占用，但未开启自动清理（service/autoCleanPorts=false）");
            appendLog("[WARN] 为避免重复拉起导致端口冲突，已中止本次脚本启动");
            return false;
        }

        appendLog("[WARN] 检测到部分端口被占用，正在尝试清理...");

        // 清理已知服务的端口
        for (int port : ports)
        {
            if (checkPortInUse(port))
            {
                killProcessByPort(port);
            }
        }

        bool stillInUse = false;
        QString occupiedAfterClean;
        for (int port : ports)
        {
            if (checkPortInUse(port))
            {
                stillInUse = true;
                if (!occupiedAfterClean.isEmpty())
                {
                    occupiedAfterClean += ",";
                }
                occupiedAfterClean += QString::number(port);
            }
        }

        if (stillInUse)
        {
            appendLog(QString("[ERROR] 端口清理后仍被占用：%1").arg(occupiedAfterClean));
            appendLog("[ERROR] 已取消本次启动，请检查占用进程或调整配置后重试");
            return false;
        }

        appendLog("[SUCCESS] 端口清理完成，允许继续启动");
        return true;
    }
    else
    {
        appendLog("[SUCCESS] 所有端口可用，无需清理");
        return true;
    }
}

void LogPage::scheduleStartRetry(const QString &reason)
{
    // 达到上限后不再重试，避免无限循环占用资源。
    if (m_startAttempt >= m_maxStartRetries)
    {
        appendLog(QString("[ERROR] 启动失败且已达到最大重试次数(%1)，停止重试。原因: %2")
                      .arg(m_maxStartRetries)
                      .arg(reason));
        return;
    }

    if (m_startRetryTimer && m_startRetryTimer->isActive())
    {
        return;
    }

    appendLog(QString("[WARN] 启动失败，%1 秒后重试。原因: %2")
                  .arg((m_startRetryTimer ? m_startRetryTimer->interval() : 0) / 1000.0, 0, 'f', 1)
                  .arg(reason));

    if (m_startRetryTimer)
    {
        m_startRetryTimer->start();
    }
}

QString LogPage::resolveServiceWorkDir() const
{
    const QString configured = ConfigManager::instance()->value("service/workDir").toString().trimmed();
    if (!configured.isEmpty() && QDir(configured).exists())
    {
        const QString configuredDir = QDir::cleanPath(configured);
        const bool hasBat = QFileInfo::exists(QDir(configuredDir).filePath("start-all.bat"));
        const bool hasSh = QFileInfo::exists(QDir(configuredDir).filePath("start-all.sh"));
        if (hasBat || hasSh)
        {
            return configuredDir;
        }
    }

    // 兼容历史目录：便于现有 Windows 联调环境直接复用后端目录
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList fallbackDirs = {
        "D:/Desktop/YunwangProject/outage/outage",
        QDir(appDir).filePath("../outage/outage"),
        QDir(appDir).filePath("../../outage/outage")};
    for (const QString &dir : fallbackDirs)
    {
        if (QDir(dir).exists())
        {
            return QDir::cleanPath(dir);
        }
    }

    if (QDir(appDir).exists())
    {
        return QDir::cleanPath(appDir);
    }

    return QString();
}

QString LogPage::resolveServiceScriptPath() const
{
    // 优先读取配置 service/scriptPath，便于 Windows/Linux 分别配置
    const QString configuredPath = ConfigManager::instance()->value("service/scriptPath").toString().trimmed();
    if (!configuredPath.isEmpty())
    {
        QFileInfo configuredInfo(configuredPath);
        if (configuredInfo.exists())
        {
            return QDir::toNativeSeparators(configuredInfo.absoluteFilePath());
        }

        // 支持相对路径：相对于 service/workDir 解析
        const QString relativePath = QDir(resolveServiceWorkDir()).filePath(configuredPath);
        QFileInfo relativeInfo(relativePath);
        if (relativeInfo.exists())
        {
            return QDir::toNativeSeparators(relativeInfo.absoluteFilePath());
        }
    }

    const QString workDir = resolveServiceWorkDir();
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList probeDirs;
    probeDirs << workDir
              << "D:/Desktop/YunwangProject/outage/outage"
              << QDir(appDir).filePath("../outage/outage")
              << QDir(appDir).filePath("../../outage/outage")
              << appDir;

    QStringList normalizedProbeDirs;
    for (const QString &dir : probeDirs)
    {
        const QString normalized = QDir::cleanPath(dir);
        if (!normalized.isEmpty() && !normalizedProbeDirs.contains(normalized))
        {
            normalizedProbeDirs.append(normalized);
        }
    }

    QStringList candidates;
#ifdef Q_OS_WIN
    candidates << "start-all.bat";
#else
    candidates << "start-all.sh" << "start-all.bat";
#endif

    for (const QString &dir : normalizedProbeDirs)
    {
        for (const QString &name : candidates)
        {
            const QString fullPath = QDir(dir).filePath(name);
            if (QFileInfo::exists(fullPath))
            {
                return QDir::toNativeSeparators(QFileInfo(fullPath).absoluteFilePath());
            }
        }
    }

    return QString();
}

QList<int> LogPage::resolveServicePorts() const
{
    // 默认仅包含项目常见端口，避免误杀系统服务端口
    const QString defaultPorts = "8081,9000,9001";
    const QString raw = ConfigManager::instance()->value("service/ports", defaultPorts).toString();
    const QStringList parts = raw.split(",", QString::SkipEmptyParts);

    QList<int> ports;
    for (const QString &part : parts)
    {
        bool ok = false;
        const int port = part.trimmed().toInt(&ok);
        if (ok && port > 0 && port <= 65535)
        {
            ports.append(port);
        }
    }

    if (ports.isEmpty())
    {
        ports << 8081 << 9000 << 9001;
    }
    return ports;
}

QList<int> LogPage::resolveRemoteRequiredPorts() const
{
    // remote 严格检查应聚焦“业务关键端口”，避免将非关键端口纳入必达条件而误判。
    // 默认只检查 API 与对象存储访问关键端口。
    const QString defaultRequiredPorts = "8081,9000";
    const QString raw = ConfigManager::instance()->value("service/remoteRequiredPorts", defaultRequiredPorts).toString();
    const QStringList parts = raw.split(",", QString::SkipEmptyParts);

    QList<int> ports;
    for (const QString &part : parts)
    {
        bool ok = false;
        const int port = part.trimmed().toInt(&ok);
        if (ok && port > 0 && port <= 65535)
        {
            ports.append(port);
        }
    }

    if (ports.isEmpty())
    {
        ports << 8081 << 9000;
    }
    return ports;
}

bool LogPage::isAutoCleanPortsEnabled() const
{
    return ConfigManager::instance()->value("service/autoCleanPorts", false).toBool();
}

bool LogPage::startScriptProcess(const QString &scriptPath)
{
    if (!m_process)
    {
        return false;
    }

#ifdef Q_OS_WIN
    m_process->start("cmd.exe", QStringList() << "/c" << scriptPath);
#else
    m_process->start("/bin/sh", QStringList() << scriptPath);
#endif
    return true;
}

QString LogPage::serviceStartMode() const
{
    // 运行时兜底默认值按平台区分，避免配置缺失时行为不符合部署目标。
#ifdef Q_OS_WIN
    const QString defaultMode = "local";
#else
    const QString defaultMode = "remote";
#endif

    QString mode = ConfigManager::instance()->value("service/startMode", defaultMode).toString().trimmed().toLower();
    if (mode != "local" && mode != "remote")
    {
        mode = defaultMode;
    }
    return mode;
}

bool LogPage::checkRemoteServiceReady(QString *detail) const
{
    auto checkUrlReachable = [](const QString &urlText, QString *err) -> bool
    {
        const QUrl url = QUrl::fromUserInput(urlText.trimmed());
        if (!url.isValid() || url.host().isEmpty() || url.scheme().isEmpty())
        {
            if (err)
            {
                *err = QString("URL 无效: %1").arg(urlText);
            }
            return false;
        }

        const int port = url.port(url.scheme() == "https" ? 443 : 80);
        QTcpSocket socket;
        socket.connectToHost(url.host(), port);
        const bool connected = socket.waitForConnected(800);
        socket.disconnectFromHost();

        if (!connected && err)
        {
            *err = QString("%1:%2 不可达").arg(url.host()).arg(port);
        }
        return connected;
    };

    QString reason;
    const QString homeUrl = ConfigManager::instance()->homeUrl();
    if (!checkUrlReachable(homeUrl, &reason))
    {
        if (detail)
        {
            *detail = QString("homeUrl 检查失败，%1").arg(reason);
        }
        return false;
    }

    const QString apiUrl = ConfigManager::instance()->apiUrl();
    if (!checkUrlReachable(apiUrl, &reason))
    {
        if (detail)
        {
            *detail = QString("apiUrl 检查失败，%1").arg(reason);
        }
        return false;
    }

    // 可选严格检查：
    // 在 Windows 联调场景，80 端口可达并不代表后端 Java/MinIO 已就绪。
    // 因此额外检查 service/ports 中的关键端口，避免“误判可达导致不回退拉起”。
    const bool requirePortsReady = ConfigManager::instance()->value("service/remoteRequirePorts", false).toBool();
    if (requirePortsReady)
    {
        const QList<int> ports = resolveRemoteRequiredPorts();
        QString missingPorts;
        for (int port : ports)
        {
            if (!checkPortInUse(port))
            {
                if (!missingPorts.isEmpty())
                {
                    missingPorts += ",";
                }
                missingPorts += QString::number(port);
            }
        }

        if (!missingPorts.isEmpty())
        {
            if (detail)
            {
                *detail = QString("关键端口不可达: %1").arg(missingPorts);
            }
            return false;
        }
    }

    if (detail)
    {
        if (requirePortsReady)
        {
            *detail = "homeUrl/apiUrl 可达，且关键端口均可达";
        }
        else
        {
            *detail = "homeUrl 与 apiUrl 均可达";
        }
    }
    return true;
}

void LogPage::startExternalLogTailing()
{
    if (m_externalLogTailingStarted)
    {
        return;
    }

    const bool enabled = ConfigManager::instance()->value("service/externalLogTailEnabled", true).toBool();
    if (!enabled)
    {
        return;
    }

    const QStringList files = resolveExternalLogFiles();
    if (files.isEmpty())
    {
        appendLog("[INFO] 未配置可跟踪的外部日志文件（可配置 service/logFiles）");
        return;
    }

    m_externalLogFiles = files;
    m_externalLogOffsets.clear();

    // 首次附着默认从文件末尾开始，避免一次性刷屏历史日志。
    for (const QString &path : m_externalLogFiles)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
        {
            m_externalLogOffsets.insert(path, file.size());
            file.close();
        }
        else
        {
            m_externalLogOffsets.insert(path, 0);
        }
    }

    int intervalMs = ConfigManager::instance()->value("service/externalLogTailIntervalMs", 1000).toInt();
    if (intervalMs < 300)
    {
        intervalMs = 300;
    }
    m_externalLogTimer->setInterval(intervalMs);
    m_externalLogTimer->start();
    m_externalLogTailingStarted = true;

    appendLog(QString("[INFO] 已启用外部日志跟踪，文件数=%1，轮询间隔=%2ms")
                  .arg(m_externalLogFiles.size())
                  .arg(intervalMs));
}

void LogPage::stopExternalLogTailing()
{
    if (m_externalLogTimer)
    {
        m_externalLogTimer->stop();
    }
    m_externalLogFiles.clear();
    m_externalLogOffsets.clear();
    m_externalLogTailingStarted = false;
}

void LogPage::pollExternalLogFiles()
{
    if (m_externalLogFiles.isEmpty())
    {
        return;
    }

    for (const QString &path : m_externalLogFiles)
    {
        QFile file(path);
        if (!file.exists())
        {
            continue;
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            continue;
        }

        qint64 offset = m_externalLogOffsets.value(path, 0);
        if (offset < 0)
        {
            offset = 0;
        }

        // 文件被覆盖/截断后，自动回退到头部继续读取。
        if (file.size() < offset)
        {
            offset = 0;
        }

        if (!file.seek(offset))
        {
            file.close();
            continue;
        }

        const QByteArray data = file.readAll();
        offset = file.pos();
        file.close();
        m_externalLogOffsets.insert(path, offset);

        if (data.isEmpty())
        {
            continue;
        }

        const QString text = QString::fromLocal8Bit(data);
        const QStringList lines = text.split("\n", QString::SkipEmptyParts);
        const QString fileName = QFileInfo(path).fileName();
        for (const QString &line : lines)
        {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty())
            {
                appendLog(QString("[EXTLOG][%1] %2").arg(fileName, trimmed));
            }
        }
    }
}

QStringList LogPage::resolveExternalLogFiles() const
{
    QString raw = ConfigManager::instance()->value("service/logFiles", "").toString();
    raw.replace("\r", "\n");
    raw.replace(";", "\n");
    raw.replace(",", "\n");

    QStringList configuredItems = raw.split("\n", QString::SkipEmptyParts);
    for (QString &item : configuredItems)
    {
        item = item.trimmed();
    }

    QStringList candidates;
    for (const QString &item : configuredItems)
    {
        if (item.isEmpty())
        {
            continue;
        }

        QFileInfo info(item);
        if (info.isAbsolute())
        {
            candidates << QDir::toNativeSeparators(info.absoluteFilePath());
            continue;
        }

        const QString workDir = resolveServiceWorkDir();
        if (!workDir.isEmpty())
        {
            const QString fullPath = QDir(workDir).filePath(item);
            candidates << QDir::toNativeSeparators(QFileInfo(fullPath).absoluteFilePath());
        }
    }

    if (candidates.isEmpty())
    {
        const QString workDir = resolveServiceWorkDir();
        const QString appDir = QCoreApplication::applicationDirPath();

        QStringList fallbackCandidates;
        if (!workDir.isEmpty())
        {
            fallbackCandidates << QDir(workDir).filePath("service_startup.log")
                               << QDir(workDir).filePath("logs/application.log")
                               << QDir(workDir).filePath("logs/kids-api.log")
                               << QDir(workDir).filePath("logs/backend.log");
        }
        fallbackCandidates << QDir(appDir).filePath("service_startup.log")
                           << QDir(appDir).filePath("../service_startup.log")
                           << QDir(appDir).filePath("../../service_startup.log");

        for (const QString &path : fallbackCandidates)
        {
            candidates << QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
        }
    }

    QStringList result;
    QSet<QString> seen;
    for (const QString &path : candidates)
    {
        const QString normalized = QDir::cleanPath(path);
        if (normalized.isEmpty() || seen.contains(normalized))
        {
            continue;
        }
        if (!QFileInfo::exists(normalized))
        {
            continue;
        }
        seen.insert(normalized);
        result << normalized;
    }

    return result;
}
