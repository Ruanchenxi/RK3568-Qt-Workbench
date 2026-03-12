/**
 * @file ProcessService.cpp
 * @brief 服务进程治理实现
 */

#include "platform/process/ProcessService.h"

#include "core/ConfigManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QTcpSocket>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>

ProcessService::ProcessService(QObject *parent)
    : IProcessService(parent),
      m_process(nullptr),
      m_autoStartTimer(new QTimer(this)),
      m_startRetryTimer(new QTimer(this)),
      m_externalLogTimer(new QTimer(this)),
      m_externalLogFiles(),
      m_externalLogOffsets(),
      m_startAttempt(0),
      m_maxStartRetries(3),
      m_startInProgress(false),
      m_externalLogTailingStarted(false)
{
    m_autoStartTimer->setSingleShot(true);
    m_autoStartTimer->setInterval(1000);
    connect(m_autoStartTimer, &QTimer::timeout, this, &ProcessService::autoStartServices);

    m_startRetryTimer->setSingleShot(true);
    m_startRetryTimer->setInterval(2000);
    connect(m_startRetryTimer, &QTimer::timeout, this, &ProcessService::onStartRetryTimeout);

    m_externalLogTimer->setSingleShot(false);
    m_externalLogTimer->setInterval(1000);
    connect(m_externalLogTimer, &QTimer::timeout, this, &ProcessService::pollExternalLogFiles);
}

ProcessService::~ProcessService()
{
    stopAll();
}

void ProcessService::startAuto()
{
    appendLog("=== 服务日志 ===");
    appendLog("等待启动服务...");
    m_autoStartTimer->start();
}

void ProcessService::stopAll()
{
    stopExternalLogTailing();

    if (m_startRetryTimer)
    {
        m_startRetryTimer->stop();
    }

    if (m_autoStartTimer)
    {
        m_autoStartTimer->stop();
    }

    if (m_process)
    {
        if (m_process->state() == QProcess::Running)
        {
            m_process->terminate();
            if (!m_process->waitForFinished(3000))
            {
                // 进程在终止超时后仍存活时强制 kill，避免 QProcess 析构警告。
                m_process->kill();
                m_process->waitForFinished(2000);
            }
        }
        delete m_process;
        m_process = nullptr;
    }

    m_startInProgress = false;
}

void ProcessService::appendLog(const QString &text)
{
    emit logGenerated(text);
}

void ProcessService::autoStartServices()
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

void ProcessService::startServices()
{
    if (m_startInProgress)
    {
        appendLog("[INFO] 启动流程进行中，忽略重复启动请求");
        return;
    }

    if (m_process && m_process->state() == QProcess::Running)
    {
        appendLog("[WARN] 服务进程已在运行中，跳过启动");
        return;
    }

    if (!m_process)
    {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::started, this, &ProcessService::onProcessStarted);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessService::onProcessReadyReadOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &ProcessService::onProcessReadyReadError);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                    onProcessFinished(exitCode, static_cast<int>(exitStatus));
                });
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            onProcessError(static_cast<int>(error));
        });
    }

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

    m_process->setWorkingDirectory(workDir);
    m_process->setReadChannel(QProcess::StandardOutput);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    env.insert("CHCP", "65001");
#endif
    m_process->setProcessEnvironment(env);

    appendLog(QString("[INFO] 启动脚本: %1").arg(scriptPath));
    appendLog(QString("[INFO] 工作目录: %1").arg(workDir));
    appendLog(QString("[INFO] 执行命令: %1\n").arg(scriptPath));

    stopExternalLogTailing();
    ++m_startAttempt;
    m_startInProgress = true;
    appendLog(QString("[INFO] 第 %1/%2 次启动尝试").arg(m_startAttempt).arg(m_maxStartRetries));

    if (!startScriptProcess(scriptPath))
    {
        m_startInProgress = false;
        appendLog("[ERROR] 启动脚本失败：进程无法启动");
        scheduleStartRetry("QProcess::start 调用失败");
        return;
    }

    appendLog("[INFO] 启动命令已提交，等待进程回调...\n");
}

void ProcessService::onProcessStarted()
{
    m_startInProgress = false;
    m_startAttempt = 0;
    appendLog("[SUCCESS] 服务脚本已启动，正在执行...\n");
}

void ProcessService::onStartRetryTimeout()
{
    appendLog("[INFO] 开始执行延迟重试...");
    startServices();
}

void ProcessService::onProcessReadyReadOutput()
{
    if (!m_process)
    {
        return;
    }

    const QByteArray data = m_process->readAllStandardOutput();
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (!codec)
    {
        codec = QTextCodec::codecForName("UTF8");
    }

    const QString output = codec ? codec->toUnicode(data) : QString::fromUtf8(data);
    const QStringList lines = output.split("\n", QString::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            appendLog(trimmed);
        }
    }
}

void ProcessService::onProcessReadyReadError()
{
    if (!m_process)
    {
        return;
    }

    const QByteArray data = m_process->readAllStandardError();
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (!codec)
    {
        codec = QTextCodec::codecForName("GB18030");
    }

    const QString error = codec ? codec->toUnicode(data) : QString::fromLocal8Bit(data);
    const QStringList lines = error.split("\n", QString::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.contains("Active code page"))
        {
            appendLog(QString("[STDERR] %1").arg(trimmed));
        }
    }
}

void ProcessService::onProcessFinished(int exitCode, int exitStatus)
{
    m_startInProgress = false;
    const QProcess::ExitStatus status = static_cast<QProcess::ExitStatus>(exitStatus);
    const QString statusText = (status == QProcess::NormalExit) ? "正常退出" : "异常终止";

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

    startExternalLogTailing();
}

void ProcessService::onProcessError(int error)
{
    m_startInProgress = false;

    QString errorMsg;
    const QProcess::ProcessError processError = static_cast<QProcess::ProcessError>(error);
    switch (processError)
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
    if (processError == QProcess::FailedToStart || processError == QProcess::Crashed)
    {
        scheduleStartRetry(errorMsg);
    }
}

bool ProcessService::checkPortInUse(int port) const
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", port);
    const bool connected = socket.waitForConnected(100);
    socket.disconnectFromHost();
    return connected;
}

void ProcessService::killProcessByPort(int port)
{
    appendLog(QString("[INFO] 正在清理端口 %1...").arg(port));

#ifdef Q_OS_WIN
    QProcess findProcess;
    findProcess.start("cmd.exe", QStringList() << "/c" << QString("netstat -ano | findstr :%1").arg(port));
    if (!findProcess.waitForFinished(3000))
    {
        findProcess.kill();
        findProcess.waitForFinished(1000);
    }

    const QString output = QString::fromLocal8Bit(findProcess.readAllStandardOutput());
    const QStringList lines = output.split("\n", QString::SkipEmptyParts);

    QSet<QString> pidsToKill;
    for (const QString &line : lines)
    {
        const QStringList parts = line.simplified().split(" ");
        if (parts.size() >= 5)
        {
            const QString pid = parts.last().trimmed();
            if (!pid.isEmpty() && pid != "0")
            {
                pidsToKill.insert(pid);
            }
        }
    }

    const QStringList whitelist = resolveKillProcessWhitelist();
    for (const QString &pid : pidsToKill)
    {
        const QString processName = queryProcessNameByPid(pid);
        if (processName.isEmpty())
        {
            appendLog(QString("[WARN] 无法识别 PID=%1 的进程名，拒绝自动终止").arg(pid));
            continue;
        }

        if (!whitelist.contains(processName.toLower()))
        {
            appendLog(QString("[WARN] PID=%1 进程 %2 不在白名单，拒绝自动终止")
                          .arg(pid, processName));
            continue;
        }

        appendLog(QString("[INFO] 正在终止进程 PID=%1...").arg(pid));
        QProcess killProcess;
        killProcess.start("cmd.exe", QStringList() << "/c" << QString("taskkill /F /PID %1").arg(pid));
        if (!killProcess.waitForFinished(2000))
        {
            killProcess.kill();
            killProcess.waitForFinished(1000);
        }

        const QString result = QString::fromLocal8Bit(killProcess.readAllStandardOutput());
        if (result.contains("SUCCESS") || result.contains("成功"))
        {
            appendLog(QString("[SUCCESS] 已终止进程 PID=%1").arg(pid));
        }
    }
#else
    QProcess killProcess;
    killProcess.start("/bin/sh", QStringList() << "-c" << QString("fuser -k %1/tcp").arg(port));
    if (!killProcess.waitForFinished(3000))
    {
        killProcess.kill();
        killProcess.waitForFinished(1000);
    }
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

QString ProcessService::queryProcessNameByPid(const QString &pid) const
{
#ifdef Q_OS_WIN
    QProcess queryProcess;
    queryProcess.start("cmd.exe", QStringList() << "/c"
                                               << QString("tasklist /FI \"PID eq %1\" /FO CSV /NH").arg(pid));
    if (!queryProcess.waitForFinished(2000))
    {
        queryProcess.kill();
        queryProcess.waitForFinished(1000);
    }
    const QString output = QString::fromLocal8Bit(queryProcess.readAllStandardOutput()).trimmed();
    if (output.isEmpty() || output.contains("INFO:", Qt::CaseInsensitive))
    {
        return QString();
    }

    QString firstLine = output.split("\n", QString::SkipEmptyParts).value(0).trimmed();
    if (firstLine.isEmpty())
    {
        return QString();
    }

    // tasklist CSV 第一列为 Image Name，通常格式如："java.exe","1234",...
    if (firstLine.startsWith("\""))
    {
        firstLine.remove(0, 1);
    }
    const int endQuote = firstLine.indexOf('"');
    if (endQuote > 0)
    {
        return firstLine.left(endQuote).trimmed();
    }

    const QStringList parts = firstLine.split(",", QString::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return QString();
    }
    QString processName = parts.first();
    processName.remove('"');
    return processName.trimmed();
#else
    Q_UNUSED(pid);
    return QString();
#endif
}

QStringList ProcessService::resolveKillProcessWhitelist() const
{
    QString raw = ConfigManager::instance()->value(
                      "service/killProcessNameWhitelist",
                      "java.exe,minio.exe,redis-server.exe,nginx.exe,mysqld.exe,node.exe,python.exe")
                      .toString();
    raw.replace(";", ",");
    const QStringList parts = raw.split(",", QString::SkipEmptyParts);

    QStringList whitelist;
    for (const QString &part : parts)
    {
        const QString normalized = part.trimmed().toLower();
        if (!normalized.isEmpty() && !whitelist.contains(normalized))
        {
            whitelist.append(normalized);
        }
    }
    return whitelist;
}

bool ProcessService::checkAndCleanPorts()
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

    if (!anyPortInUse)
    {
        appendLog("[SUCCESS] 所有端口可用，无需清理");
        return true;
    }

    if (!allowAutoClean)
    {
        appendLog("[WARN] 检测到端口占用，但未开启自动清理（service/autoCleanPorts=false）");
        appendLog("[WARN] 为避免重复拉起导致端口冲突，已中止本次脚本启动");
        return false;
    }

    appendLog("[WARN] 检测到部分端口被占用，正在尝试清理...");
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

void ProcessService::scheduleStartRetry(const QString &reason)
{
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

QString ProcessService::resolveServiceWorkDir() const
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

QString ProcessService::resolveServiceScriptPath() const
{
    const QString configuredPath = ConfigManager::instance()->value("service/scriptPath").toString().trimmed();
    if (!configuredPath.isEmpty())
    {
        QFileInfo configuredInfo(configuredPath);
        if (configuredInfo.exists())
        {
            return QDir::toNativeSeparators(configuredInfo.absoluteFilePath());
        }

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

QList<int> ProcessService::resolveServicePorts() const
{
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

QList<int> ProcessService::resolveRemoteRequiredPorts() const
{
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

bool ProcessService::isAutoCleanPortsEnabled() const
{
    return ConfigManager::instance()->value("service/autoCleanPorts", false).toBool();
}

bool ProcessService::startScriptProcess(const QString &scriptPath)
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

QString ProcessService::serviceStartMode() const
{
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

bool ProcessService::checkRemoteServiceReady(QString *detail) const
{
    auto checkUrlReachable = [](const QString &urlText, QString *err) -> bool {
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

void ProcessService::startExternalLogTailing()
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

    appendLog(QString("[INFO] 已启用外部日志跟踪，文件数=%1，轮询间隔=%2ms").arg(m_externalLogFiles.size()).arg(intervalMs));
}

void ProcessService::stopExternalLogTailing()
{
    if (m_externalLogTimer)
    {
        m_externalLogTimer->stop();
    }
    m_externalLogFiles.clear();
    m_externalLogOffsets.clear();
    m_externalLogTailingStarted = false;
}

void ProcessService::pollExternalLogFiles()
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

QStringList ProcessService::resolveExternalLogFiles() const
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
