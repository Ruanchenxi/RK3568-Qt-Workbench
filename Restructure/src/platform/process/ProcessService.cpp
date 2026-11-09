#include "platform/process/ProcessService.h"
#include "core/ConfigManager.h"
#include "platform/logging/LogService.h"
#include "platform/process/ServiceStartupPolicy.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QTcpSocket>
#include <QTextCodec>
#include <QThread>
#include <QTimer>
#include <QUrl>
namespace {
QString norm(QString v) {
  v = QDir::cleanPath(v).toLower();
  v.replace(QChar(92), QChar('/'));
  return v;
}
QString psQuote(QString v) {
  v.replace(QChar(39), QString(2, QChar(39)));
  return QStringLiteral("'%1'").arg(v);
}
QString decode(const QByteArray &d) {
  QTextCodec *c = QTextCodec::codecForName("UTF-8");
  if (c) {
    QTextCodec::ConverterState s;
    QString t = c->toUnicode(d.constData(), d.size(), &s);
    if (!s.invalidChars)
      return t;
  }
  return QString::fromLocal8Bit(d);
}
QStringList lines(QString t) {
  t.replace("\r\n", "\n");
  t.replace('\r', '\n');
  QStringList r;
  for (QString s : t.split('\n', QString::SkipEmptyParts)) {
    s = s.trimmed();
    if (!s.isEmpty())
      r << s;
  }
  return r;
}
bool important(const QString &s) {
  QString l = s.toLower();
  return l.contains("warn") || l.contains("error") || l.contains("fatal") ||
         l.contains("exception") || l.contains("failed");
}
bool run(QProcess *p, const QString &program, const QStringList &args,
         int timeout) {
  p->start(program, args);
  if (!p->waitForStarted(3000))
    return false;
  if (!p->waitForFinished(timeout)) {
    p->kill();
    p->waitForFinished(2000);
    return false;
  }
  return p->exitStatus() == QProcess::NormalExit;
}
} // namespace
ProcessService::ProcessService(QObject *parent)
    : IProcessService(parent), m_process(nullptr),
      m_autoStartTimer(new QTimer(this)), m_startRetryTimer(new QTimer(this)),
      m_readinessTimer(new QTimer(this)), m_logFileFlushTimer(new QTimer(this)),
      m_networkManager(new QNetworkAccessManager(this)), m_startAttempt(0),
      m_maxStartRetries(3), m_readinessAttempt(0), m_maxReadinessAttempts(120),
      m_startInProgress(false), m_stopping(false), m_shutdownCompleted(false),
      m_localMode(false), m_modeResolved(false), m_serviceReady(false),
      m_probeInFlight(false), m_readinessSlowPolling(false),
      m_readinessGeneration(0),
      m_serviceReadyMessage(QStringLiteral("后台服务未就绪")) {
  m_autoStartTimer->setSingleShot(true);
  m_autoStartTimer->setInterval(300);
  connect(m_autoStartTimer, &QTimer::timeout, this,
          &ProcessService::autoStartServices);
  m_startRetryTimer->setSingleShot(true);
  m_startRetryTimer->setInterval(2000);
  connect(m_startRetryTimer, &QTimer::timeout, this,
          &ProcessService::onStartRetryTimeout);
  m_readinessTimer->setSingleShot(true);
  m_readinessTimer->setInterval(1000);
  connect(m_readinessTimer, &QTimer::timeout, this,
          &ProcessService::onServiceReadinessTimeout);
  m_logFileFlushTimer->setSingleShot(true);
  m_logFileFlushTimer->setInterval(200);
  connect(m_logFileFlushTimer, &QTimer::timeout, this,
          &ProcessService::flushServiceStartupLog);
}
ProcessService::~ProcessService() {
  stopAll();
  flushServiceStartupLog();
}
bool ProcessService::isServiceReady() const { return m_serviceReady; }
QString ProcessService::serviceReadyMessage() const {
  return m_serviceReadyMessage;
}
bool ProcessService::usesLocalService() const {
  bool local = false;
  return resolveLocalMode(&local) && local;
}
void ProcessService::setServiceReady(bool ready, const QString &message) {
  QString m = message.trimmed();
  if (m.isEmpty())
    m = ready ? "后台服务已就绪" : "后台服务未就绪";
  if (m_serviceReady == ready && m_serviceReadyMessage == m)
    return;
  m_serviceReady = ready;
  m_serviceReadyMessage = m;
  emit serviceReadyChanged(ready, m);
}
void ProcessService::startAuto() {
  m_stopping = false;
  m_shutdownCompleted = false;
  m_modeResolved = false;
  m_localMode = false;
  resetServiceStartupLog();
  appendLog("=== 服务日志 ===");
  setServiceReady(false, "正在识别服务模式");
  m_autoStartTimer->start();
}
bool ProcessService::resolveLocalMode(bool *local, QString *error) const {
  bool l = false;
  QString e;
  bool ok = ConfigManager::resolveServiceTarget(
      ConfigManager::instance()->homeUrl(), ConfigManager::instance()->apiUrl(),
      &l, &e);
  if (local)
    *local = ok && l;
  if (error)
    *error = e;
  return ok;
}
void ProcessService::autoStartServices() {
  QString e;
  if (!resolveLocalMode(&m_localMode, &e)) {
    m_modeResolved = false;
    setServiceReady(false, "服务地址配置无效");
    appendLog("[ERROR] invalid service URL: " + e);
    return;
  }
  m_modeResolved = true;
  appendLog(
      QString("[INFO] 服务模式：%1").arg(m_localMode ? "local" : "remote"));
  if (!m_localMode) {
    setServiceReady(true, "指定 URL 远程服务模式");
    appendLog("[INFO] 指定 URL 模式：不启动、不停止、不清理本地 outage");
    return;
  }
  if (!ConfigManager::instance()->autoStart()) {
    setServiceReady(false, "本地服务自动启动已关闭");
    return;
  }
  startLocalServices();
}
void ProcessService::startLocalServices() {
  if (m_startInProgress || m_stopping)
    return;
  QString wd = resolveServiceWorkDir(), script = resolveServiceScriptPath();
  if (wd.isEmpty() || script.isEmpty()) {
    setServiceReady(false, "未找到本地 outage 启动脚本");
    appendLog("[ERROR] 未找到本地 outage 启动脚本");
    return;
  }
  setServiceReady(false, "正在清理并启动本地 outage");
  if (!prepareLocalRuntime()) {
    setServiceReady(false, "本地 outage 启动前检查失败");
    return;
  }
  m_startedOutageWorkDir = wd;
  disposeScriptProcess();
  m_process = new QProcess(this);
  connect(m_process, &QProcess::started, this,
          &ProcessService::onProcessStarted);
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &ProcessService::onProcessReadyReadOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &ProcessService::onProcessReadyReadError);
  connect(
      m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this,
      [this](int c, QProcess::ExitStatus s) { onProcessFinished(c, int(s)); });
  connect(m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError e) { onProcessError(int(e)); });
  m_process->setWorkingDirectory(wd);
  m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
  ++m_startAttempt;
  m_startInProgress = true;
  appendLog(QString("[INFO] 启动脚本：%1（第 %2/%3 次）")
                .arg(script)
                .arg(m_startAttempt)
                .arg(m_maxStartRetries));
  if (!startScriptProcess(script)) {
    m_startInProgress = false;
    appendLog("[ERROR] 启动脚本运行失败");
    scheduleStartRetry("launch failed");
  }
}
// Blocks foreign listeners before any cleanup and restarts only verified product outage roots.
bool ProcessService::prepareLocalRuntime() {
#ifdef Q_OS_WIN
  const QString workDir = resolveServiceWorkDir();
  ServiceStartupPolicy::Observation observation;
  QSet<int> ownedPorts;
  QSet<int> foreignPorts;
  for (int port : resolveServicePorts()) {
    if (!checkPortInUse(port))
      continue;
    observation.occupiedPorts << port;
    const QList<ProcessOwner> owners = queryWindowsPortOwners(port);
    if (owners.isEmpty()) {
      foreignPorts << port;
      appendLog(QString("[ERROR] 端口 %1 已被占用，但无法获取占用进程信息")
                    .arg(port));
      continue;
    }
    bool allOwned = true;
    for (const ProcessOwner &owner : owners) {
      const QString outageRoot =
          windowsOutageRootForProcess(owner, workDir);
      const bool owned = !outageRoot.isEmpty();
      QString detail = describeProcessOwner(owner);
      if (owned)
        detail += QString(" outageRoot=%1").arg(outageRoot);
      appendLog(QString("[%1] port %2 %3")
                    .arg(owned ? "INFO" : "BLOCK")
                    .arg(port)
                    .arg(detail));
      if (!owned)
        allOwned = false;
    }
    (allOwned ? ownedPorts : foreignPorts) << port;
  }
  observation.ownedOutagePorts = ownedPorts.values();
  observation.foreignProcessPorts = foreignPorts.values();
  if (ServiceStartupPolicy::decide(observation).action ==
      ServiceStartupPolicy::Action::Block) {
    appendLog("[ERROR] 必要端口被外部进程占用，未清理、未启动本地 outage");
    return false;
  }
  if (!stopWindowsOutage(true)) {
    appendLog("[ERROR] 上一套本地 outage 未能完整停止");
    return false;
  }
#else
  if (!stopLocalOutage()) {
    appendLog("[ERROR] 上一套本地 outage 未能完整停止");
    return false;
  }
#endif
  const QList<int> ports = resolveServicePorts();
  if (!waitForPortsReleased(ports, 5000)) {
    appendLog("[ERROR] 清理后受管端口仍被占用");
    return false;
  }
  appendLog("[SUCCESS] 上一套 outage 已停止，受管端口已释放");
  return true;
}
void ProcessService::stopAll() {
  if (m_stopping || m_shutdownCompleted)
    return;
  m_stopping = true;
  m_autoStartTimer->stop();
  m_startRetryTimer->stop();
  m_readinessTimer->stop();
  ++m_readinessGeneration;
  m_probeInFlight = false;
  for (QNetworkReply *r : m_networkManager->findChildren<QNetworkReply *>())
    if (r->isRunning())
      r->abort();
  m_startInProgress = false;
  setServiceReady(false, "后台服务已停止");
  bool local = m_localMode;
  if (!m_modeResolved)
    resolveLocalMode(&local);
  if (local) {
    appendLog("[INFO] 正在停止整套本地 outage");
    if (stopLocalOutage())
      appendLog("[SUCCESS] 本地 outage 已停止");
    else
      appendLog("[ERROR] 本地 outage 未能完整停止");
  } else
    appendLog("[INFO] 指定 URL 模式：不启动、不停止、不清理本地 outage");
  disposeScriptProcess();
  m_startedOutageWorkDir.clear();
  flushServiceStartupLog();
  m_shutdownCompleted = true;
  m_stopping = false;
}
bool ProcessService::stopLocalOutage() {
#ifdef Q_OS_WIN
  return stopWindowsOutage(false);
#else
  return stopLinuxOutage();
#endif
}
bool ProcessService::runStopScript(const QString &path) {
  if (path.isEmpty())
    return false;
  QProcess p;
#ifdef Q_OS_WIN
  bool ok =
      run(&p, "cmd.exe", QStringList() << "/d" << "/s" << "/c" << path, 20000);
#else
  bool ok = run(&p, "/bin/bash", QStringList() << path, 15000);
#endif
  for (QString s : lines(decode(p.readAllStandardOutput())))
    appendLog("[STOP] " + s);
  for (QString s : lines(decode(p.readAllStandardError())))
    appendLog("[STOP][STDERR] " + s);
  return ok && p.exitCode() == 0;
}
// Stops whole verified outage roots so a child listener cannot be recreated by its parent.
bool ProcessService::stopWindowsOutage(bool includeDetectedRoots) {
#ifdef Q_OS_WIN
  const QString configuredRoot = m_startedOutageWorkDir.isEmpty()
                                     ? resolveServiceWorkDir()
                                     : m_startedOutageWorkDir;
  QSet<QString> roots;
  if (!configuredRoot.isEmpty())
    roots << QDir::cleanPath(QFileInfo(configuredRoot).absoluteFilePath());

  if (includeDetectedRoots) {
    for (int port : resolveServicePorts()) {
      for (const ProcessOwner &owner : queryWindowsPortOwners(port)) {
        const QString outageRoot =
            windowsOutageRootForProcess(owner, configuredRoot);
        if (!outageRoot.isEmpty())
          roots << outageRoot;
      }
    }
  }

  if (roots.isEmpty()) {
    appendLog("[INFO] 未发现需要停止的本产品 outage 根目录");
    return true;
  }

  QStringList orderedRoots = roots.values();
  orderedRoots.sort(Qt::CaseInsensitive);
  bool rootsStopped = true;
  for (const QString &outageRoot : orderedRoots) {
    if (!isWindowsOutageRoot(outageRoot, configuredRoot)) {
      rootsStopped = false;
      appendLog("[ERROR] outage 根目录校验失败，未执行停止：" + outageRoot);
      continue;
    }
    appendLog("[INFO] 正在停止本产品 outage 根目录：" + outageRoot);
    if (!terminateWindowsOutageRoot(outageRoot))
      rootsStopped = false;
  }

  if (!includeDetectedRoots)
    return rootsStopped;

  const bool portsReleased =
      waitForPortsReleased(resolveServicePorts(), 5000);
  if (!portsReleased) {
    appendLog("[ERROR] 本地 outage 停止后仍有受管端口未释放");
    for (int port : resolveServicePorts()) {
      if (!checkPortInUse(port))
        continue;
      const QList<ProcessOwner> owners = queryWindowsPortOwners(port);
      if (owners.isEmpty()) {
        appendLog(QString("[ERROR] 端口 %1 仍被占用，但无法获取进程信息")
                      .arg(port));
        continue;
      }
      for (const ProcessOwner &owner : owners)
        appendLog(QString("[ERROR] 端口 %1 残留进程：%2")
                      .arg(port)
                      .arg(describeProcessOwner(owner)));
    }
  }
  return rootsStopped && portsReleased;
#else
  Q_UNUSED(includeDetectedRoots);
  return true;
#endif
}
bool ProcessService::stopLinuxOutage() {
#ifdef Q_OS_LINUX
  QString stop = resolveStopScriptPath();
  if (!stop.isEmpty())
    return runStopScript(stop) &&
           waitForPortsReleased(resolveServicePorts(), 5000);
  QProcess p;
  QString cmd =
      "pids=$(ps -eo pid=,args=|awk '$0~/[o]utage-api\\.jar/{print $1}'); [ -z "
      "\"$pids\" ]||{ kill -TERM $pids 2>/dev/null||true;sleep 3;for x in "
      "$pids;do kill -0 $x 2>/dev/null&&kill -KILL $x;done;}";
  return run(&p, "/bin/bash", QStringList() << "-c" << cmd, 10000) &&
         waitForPortsReleased(resolveServicePorts(), 5000);
#else
  return true;
#endif
}
bool ProcessService::waitForPortsReleased(const QList<int> &ports,
                                          int timeout) const {
  for (int e = 0; e <= timeout; e += 100) {
    bool free = true;
    for (int p : ports)
      if (checkPortInUse(p)) {
        free = false;
        break;
      }
    if (free)
      return true;
    QThread::msleep(100);
  }
  return false;
}
bool ProcessService::checkPortInUse(int port) const {
  QTcpSocket s;
  s.connectToHost("127.0.0.1", quint16(port));
  bool c = s.waitForConnected(120);
  s.abort();
  return c;
}
QList<ProcessService::ProcessOwner>
ProcessService::queryWindowsPortOwners(int port) const {
  QList<ProcessOwner> out;
#ifdef Q_OS_WIN
  QProcess p;
  run(&p, "netstat.exe", QStringList() << "-ano" << "-p" << "TCP", 5000);
  QSet<QString> ids;
  const QString suffix = ":" + QString::number(port);
  for (QString line : lines(QString::fromLocal8Bit(p.readAllStandardOutput()))) {
    if (!line.contains("LISTENING", Qt::CaseInsensitive))
      continue;
    const QStringList fields =
        line.simplified().split(' ', QString::SkipEmptyParts);
    if (fields.size() >= 5 && fields.at(1).endsWith(suffix))
      ids << fields.last();
  }
  for (const QString &pid : ids) {
    const ProcessOwner owner = queryWindowsProcess(pid);
    if (!owner.pid.isEmpty())
      out << owner;
  }
#else
  Q_UNUSED(port);
#endif
  return out;
}
// Captures parent PID so a listening worker maps to its top-level process tree.
ProcessService::ProcessOwner
ProcessService::queryWindowsProcess(const QString &pid) const {
  ProcessOwner out;
#ifdef Q_OS_WIN
  QString script =
      QString(
          "$p=Get-CimInstance Win32_Process -Filter \"ProcessId=%1\" "
          "-ErrorAction SilentlyContinue;if($p){$p|select "
          "ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine|"
          "ConvertTo-Json -Compress}")
          .arg(pid);
  QProcess p;
  run(&p, "powershell.exe",
      QStringList() << "-NoProfile" << "-NonInteractive" << "-Command"
                    << script,
      5000);
  QJsonDocument d = QJsonDocument::fromJson(p.readAllStandardOutput());
  if (d.isObject()) {
    const QJsonObject o = d.object();
    out.pid = QString::number(o.value("ProcessId").toVariant().toLongLong());
    out.parentPid =
        QString::number(o.value("ParentProcessId").toVariant().toLongLong());
    out.name = o.value("Name").toString();
    out.executablePath = o.value("ExecutablePath").toString();
    out.commandLine = o.value("CommandLine").toString();
  }
#else
  Q_UNUSED(pid);
#endif
  return out;
}
// Enumerates only executable and launcher processes under one verified outage root.
QList<ProcessService::ProcessOwner>
ProcessService::queryWindowsOutageProcesses(const QString &outageRoot) const {
  QList<ProcessOwner> out;
#ifdef Q_OS_WIN
  if (outageRoot.isEmpty())
    return out;
  const QString root =
      QDir::toNativeSeparators(QFileInfo(outageRoot).absoluteFilePath());
  const QString script =
      QString(
          "$root=[IO.Path]::GetFullPath(%1).TrimEnd('\\');"
          "$prefix=$root+'\\';"
          "$p=Get-CimInstance Win32_Process -ErrorAction SilentlyContinue|?{"
          "($_.ExecutablePath -and [IO.Path]::GetFullPath($_.ExecutablePath)."
          "StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase))-or"
          "($_.CommandLine -and $_.CommandLine.IndexOf($prefix,"
          "[StringComparison]::OrdinalIgnoreCase)-ge 0 -and "
          "$_.CommandLine -match 'outage-api\\.jar|start-all\\.bat')}|"
          "select ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine|"
          "ConvertTo-Json -Compress")
          .arg(psQuote(root));
  QProcess p;
  run(&p, "powershell.exe",
      QStringList() << "-NoProfile" << "-NonInteractive" << "-Command"
                    << script,
      8000);
  const QJsonDocument d =
      QJsonDocument::fromJson(p.readAllStandardOutput());
  auto appendOwner = [&out](const QJsonObject &o) {
    ProcessOwner owner;
    owner.pid = QString::number(o.value("ProcessId").toVariant().toLongLong());
    owner.parentPid =
        QString::number(o.value("ParentProcessId").toVariant().toLongLong());
    owner.name = o.value("Name").toString();
    owner.executablePath = o.value("ExecutablePath").toString();
    owner.commandLine = o.value("CommandLine").toString();
    if (!owner.pid.isEmpty() && owner.pid != "0")
      out << owner;
  };
  if (d.isObject())
    appendOwner(d.object());
  else if (d.isArray())
    for (const QJsonValue &value : d.array())
      if (value.isObject())
        appendOwner(value.toObject());
#else
  Q_UNUSED(outageRoot);
#endif
  return out;
}
// Uses verified listener ancestry only when the full root scan cannot enumerate a live package.
QStringList ProcessService::windowsOutageListeningRootProcessIds(
    const QString &outageRoot) const {
  QStringList roots;
#ifdef Q_OS_WIN
  const QString expectedRoot = norm(outageRoot);
  if (expectedRoot.isEmpty())
    return roots;

  for (int port : resolveServicePorts()) {
    for (const ProcessOwner &listener : queryWindowsPortOwners(port)) {
      if (norm(windowsOutageRootForProcess(listener, outageRoot)) != expectedRoot)
        continue;

      ProcessOwner candidate = listener;
      QSet<QString> visited;
      visited << listener.pid;
      while (!candidate.parentPid.isEmpty() &&
             !visited.contains(candidate.parentPid)) {
        const ProcessOwner parent = queryWindowsProcess(candidate.parentPid);
        if (parent.pid.isEmpty() ||
            norm(windowsOutageRootForProcess(parent, outageRoot)) != expectedRoot)
          break;
        candidate = parent;
        visited << candidate.pid;
      }
      if (!candidate.pid.isEmpty())
        roots << candidate.pid;
    }
  }
#else
  Q_UNUSED(outageRoot);
#endif
  roots.removeDuplicates();
  roots.sort();
  return roots;
}
// Older roots require the full product package signature before they are trusted.
bool ProcessService::isWindowsOutageRoot(
    const QString &outageRoot, const QString &configuredWorkDir) const {
#ifdef Q_OS_WIN
  if (outageRoot.trimmed().isEmpty())
    return false;
  const QString root = QDir::cleanPath(QFileInfo(outageRoot).absoluteFilePath());
  const QString configured =
      QDir::cleanPath(QFileInfo(configuredWorkDir).absoluteFilePath());
  if (!configuredWorkDir.trimmed().isEmpty() &&
      norm(root) == norm(configured)) {
    return QFileInfo(QDir(root).filePath("start-all.bat")).isFile();
  }
  const QStringList required = {
      "start-all.bat",
      "nginx/nginx.exe",
      "mysql/bin/mysqld.exe",
      "redis/redis-server.exe",
      "minio/minio.exe",
      "jdk/bin/java.exe",
      "outage/outage-api.jar"};
  for (const QString &relative : required)
    if (!QFileInfo(QDir(root).filePath(relative)).isFile())
      return false;
  return true;
#else
  Q_UNUSED(outageRoot);
  Q_UNUSED(configuredWorkDir);
  return false;
#endif
}
// Derives a verified outage root from a configured process or packaged component path.
QString ProcessService::windowsOutageRootForProcess(
    const ProcessOwner &owner, const QString &configuredWorkDir) const {
#ifdef Q_OS_WIN
  const QString configured = configuredWorkDir.trimmed().isEmpty()
                                 ? QString()
                                 : QDir::cleanPath(
                                       QFileInfo(configuredWorkDir).absoluteFilePath());
  const QString executable = norm(owner.executablePath);
  QString command = owner.commandLine.toLower();
  command.replace(QChar(92), QChar('/'));
  if (!configured.isEmpty() &&
      ((!executable.isEmpty() &&
        (executable == norm(configured) ||
         executable.startsWith(norm(configured) + '/'))) ||
       (command.contains(norm(configured)) &&
        (command.contains("outage-api.jar") ||
         command.contains("start-all.bat"))))) {
    if (isWindowsOutageRoot(configured, configured))
      return configured;
  }

  if (owner.executablePath.trimmed().isEmpty())
    return QString();
  const QString name = owner.name.trimmed().toLower();
  const QString executablePath = QFileInfo(owner.executablePath).absoluteFilePath();
  QDir componentDir(QFileInfo(executablePath).absolutePath());
  QString candidate;
  auto rootFromParents = [&componentDir](int levels) {
    QDir rootDir = componentDir;
    for (int i = 0; i < levels; ++i)
      if (!rootDir.cdUp())
        return QString();
    return QDir::cleanPath(rootDir.absolutePath());
  };
  if (name == "nginx.exe" && executable.endsWith("/nginx/nginx.exe"))
    candidate = rootFromParents(1);
  else if (name == "mysqld.exe" &&
           executable.endsWith("/mysql/bin/mysqld.exe"))
    candidate = rootFromParents(2);
  else if (name == "redis-server.exe" &&
           executable.endsWith("/redis/redis-server.exe"))
    candidate = rootFromParents(1);
  else if (name == "minio.exe" &&
           executable.endsWith("/minio/minio.exe"))
    candidate = rootFromParents(1);
  else if (name == "java.exe" &&
           executable.endsWith("/jdk/bin/java.exe") &&
           command.contains("outage-api.jar"))
    candidate = rootFromParents(2);

  if (!candidate.isEmpty() && isWindowsOutageRoot(candidate, configured))
    return candidate;
#else
  Q_UNUSED(owner);
  Q_UNUSED(configuredWorkDir);
#endif
  return QString();
}
// A process is a termination root only when its parent is outside the same outage group.
QStringList ProcessService::windowsOutageRootProcessIds(
    const QList<ProcessOwner> &owners) const {
  QSet<QString> processIds;
  for (const ProcessOwner &owner : owners)
    if (!owner.pid.isEmpty())
      processIds << owner.pid;

  QStringList roots;
  for (const ProcessOwner &owner : owners)
    if (!owner.pid.isEmpty() && !processIds.contains(owner.parentPid))
      roots << owner.pid;
  if (roots.isEmpty())
    roots = processIds.values();
  roots.removeDuplicates();
  roots.sort();
  return roots;
}
// Repeats tree termination until no process remains under the verified outage root.
bool ProcessService::terminateWindowsOutageRoot(const QString &outageRoot) {
#ifdef Q_OS_WIN
  for (int attempt = 1; attempt <= 3; ++attempt) {
    const QList<ProcessOwner> owners =
        queryWindowsOutageProcesses(outageRoot);
    if (owners.isEmpty()) {
      const QStringList listenerRootPids =
          windowsOutageListeningRootProcessIds(outageRoot);
      if (listenerRootPids.isEmpty())
        return true;
      appendLog(QString("[WARN] Root process scan was empty; stopping listener trees: %1")
                    .arg(listenerRootPids.join(",")));
      for (const QString &pid : listenerRootPids) {
        QProcess taskkill;
        const bool succeeded =
            run(&taskkill, "taskkill.exe",
                QStringList() << "/PID" << pid << "/T" << "/F", 8000) &&
            taskkill.exitCode() == 0;
        if (!succeeded) {
          appendLog(QString("[WARN] Listener process tree stop failed, PID=%1; will verify again")
                        .arg(pid));
          for (const QString &line : lines(decode(taskkill.readAllStandardError())))
            appendLog("[STOP][STDERR] " + line);
        }
      }
      QThread::msleep(350);
      continue;
    }
    const QStringList rootPids = windowsOutageRootProcessIds(owners);
    appendLog(QString("[INFO] 停止根目录进程树（第 %1/3 轮）：%2")
                  .arg(attempt)
                  .arg(rootPids.join(',')));
    for (const QString &pid : rootPids) {
      QProcess taskkill;
      const bool succeeded =
          run(&taskkill, "taskkill.exe",
              QStringList() << "/PID" << pid << "/T" << "/F", 8000) &&
          taskkill.exitCode() == 0;
      if (!succeeded) {
        appendLog(QString("[WARN] 停止根进程失败，PID=%1；将再次核验")
                      .arg(pid));
        for (const QString &line : lines(decode(taskkill.readAllStandardError())))
          appendLog("[STOP][STDERR] " + line);
      }
    }
    QThread::msleep(350);
  }

  const QList<ProcessOwner> remaining =
      queryWindowsOutageProcesses(outageRoot);
  for (const ProcessOwner &owner : remaining)
    appendLog("[ERROR] outage 根目录残留进程：" + describeProcessOwner(owner));
  return remaining.isEmpty();
#else
  Q_UNUSED(outageRoot);
  return true;
#endif
}
QString ProcessService::describeProcessOwner(const ProcessOwner &o) const {
  return QString("PID=%1 parentPID=%2 name=%3 path=%4 command=%5")
      .arg(o.pid, o.parentPid.isEmpty() ? "<unknown>" : o.parentPid,
           o.name.isEmpty() ? "<unknown>" : o.name,
           o.executablePath.isEmpty() ? "<unknown>" : o.executablePath,
           o.commandLine.isEmpty() ? "<unknown>" : o.commandLine);
}
QString ProcessService::resolveServiceWorkDir() const {
#ifdef Q_OS_WIN
  QStringList c = {
      qEnvironmentVariable("RK3568_OUTAGE_DIR").trimmed(),
      ConfigManager::instance()->value("service/workDir").toString().trimmed(),
      QDir(QCoreApplication::applicationDirPath()).filePath("outage"),
      "D:/Desktop/YunwangProject/outage/outage"};
  QString file = "start-all.bat";
#else
  QStringList c = {
      ConfigManager::instance()->value("service/workDir").toString().trimmed(),
      "/home/linaro/outage", QCoreApplication::applicationDirPath()};
  QString file = "start.sh";
#endif
  for (QString p : c)
    if (!p.isEmpty() && QFileInfo::exists(QDir(p).filePath(file)))
      return QFileInfo(p).absoluteFilePath();
  return QString();
}
QString ProcessService::resolveServiceScriptPath() const {
  QString wd = resolveServiceWorkDir();
  if (wd.isEmpty())
    return QString();
  QString set = ConfigManager::instance()
                    ->value("service/scriptPath")
                    .toString()
                    .trimmed();
  QStringList c;
  if (!set.isEmpty())
    c << (QFileInfo(set).isAbsolute() ? set : QDir(wd).filePath(set));
#ifdef Q_OS_WIN
  c << QDir(wd).filePath("start-all.bat");
#else
  c << QDir(wd).filePath("start.sh");
#endif
  for (QString p : c)
    if (QFileInfo(p).isFile())
      return QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
  return QString();
}
QString ProcessService::resolveStopScriptPath() const {
  QString wd = resolveServiceWorkDir();
  if (wd.isEmpty())
    return QString();
#ifdef Q_OS_WIN
  QString p = QDir(wd).filePath("stop-all.bat");
#else
  QString p = QDir(wd).filePath("stop.sh");
#endif
  return QFileInfo::exists(p)
             ? QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath())
             : QString();
}
QList<int> ProcessService::resolveServicePorts() const {
#ifdef Q_OS_WIN
  QString def = "80,3306,6379,8081,9000,9090";
#else
  QString def = "8081";
#endif
  QList<int> out;
  for (QString x : ConfigManager::instance()
                       ->value("service/ports", def)
                       .toString()
                       .split(',', QString::SkipEmptyParts)) {
    bool ok = false;
    int p = x.trimmed().toInt(&ok);
    if (ok && p > 0 && p < 65536 && !out.contains(p))
      out << p;
  }
  if (out.isEmpty())
    out << 8081;
  return out;
}
int ProcessService::readinessPort() const {
  int p =
      ConfigManager::instance()->value("service/readinessPort", 8081).toInt();
  return p > 0 && p < 65536 ? p : 8081;
}

// Keeps the board cold-start allowance configurable without weakening the readiness checks.
int ProcessService::readinessTimeoutSeconds() const {
  return qBound(30,
                ConfigManager::instance()
                    ->value("service/readinessTimeoutSeconds", 120)
                    .toInt(),
                600);
}
bool ProcessService::startScriptProcess(const QString &p) {
  if (!m_process)
    return false;
#ifdef Q_OS_WIN
  m_process->start("cmd.exe", QStringList() << "/d" << "/s" << "/c"
                                            << QDir::toNativeSeparators(p));
#else
  m_process->start("/bin/bash", QStringList() << p);
#endif
  return true;
}
void ProcessService::onProcessStarted() {
  m_startInProgress = false;
  appendLog("[SUCCESS] 本地 outage 启动进程已运行");
  setServiceReady(false, "正在等待本地 outage 就绪");
  startReadinessCheck();
}
void ProcessService::startReadinessCheck() {
  ++m_readinessGeneration;
  m_probeInFlight = false;
  m_readinessSlowPolling = false;
  m_readinessAttempt = 0;
  m_maxReadinessAttempts = readinessTimeoutSeconds();
  m_readinessTimer->setInterval(1000);
  m_readinessTimer->start();
}

// A timeout reports slow startup rather than terminal failure while the managed process is alive.
void ProcessService::scheduleReadinessRetry(const QString &pendingReason) {
  if (m_readinessAttempt >= m_maxReadinessAttempts && !m_readinessSlowPolling) {
    m_readinessSlowPolling = true;
    m_readinessTimer->setInterval(5000);
    const QString message = QString("服务启动超过 %1 秒，后台进程仍在运行，%2，将继续检测")
                                .arg(m_maxReadinessAttempts)
                                .arg(pendingReason);
    setServiceReady(false, message);
    appendLog("[WARN] " + message);
  }
  m_readinessTimer->start();
}

void ProcessService::onServiceReadinessTimeout() {
  ++m_readinessAttempt;
  if (m_stopping)
    return;
  if (!m_process || m_process->state() == QProcess::NotRunning) {
    finishReadinessProbe(false, "outage 启动进程在就绪前退出");
    return;
  }
  QStringList missing;
  for (int p : resolveServicePorts())
    if (!checkPortInUse(p))
      missing << QString::number(p);
  if (missing.isEmpty()) {
    if (!m_probeInFlight) {
      m_probeInFlight = true;
      probeHealthEndpoint();
    }
    return;
  }
  if (!m_readinessSlowPolling
      && (m_readinessAttempt == 1 || m_readinessAttempt % 5 == 0))
    appendLog(QString("[INFO] 等待必要端口：%1（%2/%3）")
                  .arg(missing.join(','))
                  .arg(m_readinessAttempt)
                  .arg(m_maxReadinessAttempts));
  scheduleReadinessRetry("必要端口尚未监听：" + missing.join(','));
}
void ProcessService::probeHealthEndpoint() {
  QString u = ConfigManager::instance()
                  ->value("service/healthUrl")
                  .toString()
                  .trimmed();
  if (u.isEmpty())
    u = ConfigManager::instance()->homeUrl();
  int gen = m_readinessGeneration;
  QNetworkReply *r = m_networkManager->get(QNetworkRequest(QUrl(u)));
  configureProbeTimeout(r);
  connect(r, &QNetworkReply::finished, this, [this, r, gen]() {
    int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_stopping || gen != m_readinessGeneration) {
      r->deleteLater();
      return;
    }
    bool reached = r->error() == QNetworkReply::NoError || status > 0;
    r->deleteLater();
    if (!reached || status < 200 || status >= 400) {
      m_probeInFlight = false;
      scheduleReadinessRetry(QString("健康接口尚未就绪，HTTP 状态=%1").arg(status));
      return;
    }
    probeLoginEndpoint();
  });
}
void ProcessService::probeLoginEndpoint() {
  QUrl u(ConfigManager::instance()->apiUrl());
  QString p = u.path();
  if (!p.endsWith('/'))
    p += '/';
  u.setPath(p + "login-by-password");
  u.setQuery(QString());
  int gen = m_readinessGeneration;
  QNetworkReply *r =
      m_networkManager->sendCustomRequest(QNetworkRequest(u), "OPTIONS");
  configureProbeTimeout(r);
  connect(r, &QNetworkReply::finished, this, [this, r, gen]() {
    int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_stopping || gen != m_readinessGeneration) {
      r->deleteLater();
      return;
    }
    r->deleteLater();
    m_probeInFlight = false;
    bool exists = status >= 200 && status < 500 && status != 404;
    if (exists) {
      m_startAttempt = 0;
      m_readinessAttempt = 0;
      setServiceReady(true, "本地 outage 已就绪");
      appendLog("[SUCCESS] 进程、端口、健康地址和登录接口均已就绪");
    } else {
      scheduleReadinessRetry(QString("登录接口尚未就绪，HTTP 状态=%1").arg(status));
    }
  });
}
void ProcessService::finishReadinessProbe(bool ready, const QString &m) {
  m_probeInFlight = false;
  setServiceReady(ready, m);
  appendLog(QString("[%1] %2").arg(ready ? "SUCCESS" : "ERROR", m));
}
void ProcessService::configureProbeTimeout(QNetworkReply *r) {
  int t = qBound(500,
                 ConfigManager::instance()
                     ->value("service/readinessProbeTimeoutMs", 3000)
                     .toInt(),
                 10000);
  QTimer::singleShot(t, r, [r]() {
    if (r->isRunning())
      r->abort();
  });
}
void ProcessService::onProcessReadyReadOutput() {
  if (!m_process)
    return;
  for (QString s : lines(decode(m_process->readAllStandardOutput())))
    if (!m_serviceReady || important(s))
      appendLog("[START] " + s);
}
void ProcessService::onProcessReadyReadError() {
  if (!m_process)
    return;
  for (QString s : lines(decode(m_process->readAllStandardError())))
    appendLog("[START][STDERR] " + s);
}
void ProcessService::onProcessFinished(int code, int status) {
  m_startInProgress = false;
  if (m_stopping)
    return;
  bool ready = m_serviceReady;
  setServiceReady(false, "本地 outage 进程已退出");
  appendLog(QString("[ERROR] outage 异常退出：exitCode=%1 exitStatus=%2 "
                    "previouslyReady=%3")
                .arg(code)
                .arg(status)
                .arg(ready ? "true" : "false"));
  scheduleStartRetry("outage 进程退出");
}
void ProcessService::onProcessError(int e) {
  m_startInProgress = false;
  appendLog(QString("[ERROR] 启动进程错误=%1").arg(e));
  if (!m_stopping)
    scheduleStartRetry("QProcess error");
}
void ProcessService::scheduleStartRetry(const QString &r) {
  if (m_stopping || m_startAttempt >= m_maxStartRetries) {
    appendLog("[ERROR] 已停止启动重试：" + r);
    return;
  }
  if (!m_startRetryTimer->isActive()) {
    appendLog("[WARN] 2 秒后重试本地 outage：" + r);
    m_startRetryTimer->start();
  }
}
void ProcessService::onStartRetryTimeout() { startLocalServices(); }
QString ProcessService::serviceStartupLogPath() const {
  return QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath())
                                      .filePath("service_startup.log"));
}
void ProcessService::resetServiceStartupLog() {
  m_logFileFlushTimer->stop();
  m_pendingFileLogs.clear();
  QFile f(serviceStartupLogPath());
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.close();
}
void ProcessService::appendServiceStartupLogLine(const QString &t) {
  QString s = t.trimmed();
  if (s.isEmpty())
    return;
  m_pendingFileLogs << s;
  if (!m_logFileFlushTimer->isActive())
    m_logFileFlushTimer->start();
}
void ProcessService::flushServiceStartupLog() {
  if (m_pendingFileLogs.isEmpty())
    return;
  QStringList b = m_pendingFileLogs;
  m_pendingFileLogs.clear();
  QFile f(serviceStartupLogPath());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
    return;
  f.write(b.join('\n').toUtf8());
  f.write("\n");
}
void ProcessService::appendLog(const QString &t) {
  if (t.contains("[ERROR]", Qt::CaseInsensitive)) {
    LOG_ERROR("SERVICE", t);
    LOG_INCIDENT("SERVICE", t);
  } else if (t.contains("[SUCCESS]", Qt::CaseInsensitive) ||
             t.contains("[STOP]", Qt::CaseInsensitive))
    LOG_INFO("SERVICE", t);
  emit logGenerated(t);
  appendServiceStartupLogLine(t);
}
void ProcessService::disposeScriptProcess() {
  if (!m_process)
    return;
  if (m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(2000);
  }
  m_process->deleteLater();
  m_process = nullptr;
}
