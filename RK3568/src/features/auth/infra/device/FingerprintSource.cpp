/**
 * @file FingerprintSource.cpp
 * @brief 指纹采集源实现（dlopen libzazapi.so + QTimer 轮询）
 */
#include "features/auth/infra/device/FingerprintSource.h"

#include "features/auth/infra/device/zazapi.h"
#include "core/ConfigManager.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif

namespace {

constexpr int kPollIntervalMs = 500;
constexpr int kCaptureDedupeMs = 2000;
constexpr int kTemplateSize = 512;

/* SDK 路径（板端） */
const char *kSdkPaths[] = {
    "/home/linaro/AdapterService/bin/libs/libzazapi.so",
    "/home/linaro/demo/src/libzazapi.so",
    "libzazapi.so",
    nullptr
};

/* 函数指针全局实例 */
fn_ZAZ_MODE        g_ZAZ_MODE = nullptr;
fn_ZAZ_SETPATH     g_ZAZ_SETPATH = nullptr;
fn_ZAZSetlog       g_ZAZSetlog = nullptr;
fn_ZAZOpenDeviceEx g_ZAZOpenDeviceEx = nullptr;
fn_ZAZCloseDeviceEx g_ZAZCloseDeviceEx = nullptr;
fn_ZAZVfyPwd       g_ZAZVfyPwd = nullptr;
fn_ZAZGetImage     g_ZAZGetImage = nullptr;
fn_ZAZGenChar      g_ZAZGenChar = nullptr;
fn_ZAZSetCharLen   g_ZAZSetCharLen = nullptr;
fn_ZAZUpChar       g_ZAZUpChar = nullptr;
fn_ZAZRegModule    g_ZAZRegModule = nullptr;

} // namespace

FingerprintSource::FingerprintSource(QObject *parent)
    : ICredentialSource(parent)
    , m_lib(nullptr)
    , m_handle(-1)
    , m_deviceOpen(false)
    , m_pollTimer(new QTimer(this))
    , m_lastCaptureMs(0)
    , m_enrollMode(false)
    , m_enrollState(Idle)
    , m_liftConfirmCount(0)
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &FingerprintSource::poll);
}

FingerprintSource::~FingerprintSource()
{
    stop();
    closeDevice();
#ifdef Q_OS_LINUX
    if (m_lib) {
        dlclose(m_lib);
        m_lib = nullptr;
    }
#endif
}

AuthMode FingerprintSource::mode() const
{
    return AuthMode::Fingerprint;
}

void FingerprintSource::start()
{
    if (m_pollTimer->isActive()) {
        return;
    }

#ifdef Q_OS_LINUX
    if (!m_lib && !loadSdk()) {
        return;
    }

    if (!m_deviceOpen && !initDevice()) {
        return;
    }

    m_enrollMode = false;
    m_enrollState = Idle;
    m_liftConfirmCount = 0;
    m_lastCaptureMs = 0;
    m_pollTimer->start();
    qDebug() << "[FingerprintSource] 轮询已启动（登录模式）";
#else
    emit sourceError(QStringLiteral("指纹模块仅在 Linux 板端可用"));
#endif
}

void FingerprintSource::startEnroll()
{
    if (m_pollTimer->isActive()) {
        return;
    }

#ifdef Q_OS_LINUX
    if (!m_lib && !loadSdk()) {
        return;
    }

    if (!m_deviceOpen && !initDevice()) {
        return;
    }

    if (!g_ZAZRegModule) {
        emit sourceError(QStringLiteral("SDK 缺少 ZAZRegModule 函数，无法注册指纹"));
        return;
    }

    m_enrollMode = true;
    m_enrollState = WaitPress1;
    m_liftConfirmCount = 0;
    m_lastCaptureMs = 0;
    m_stepTimer.start();
    m_pollTimer->start();
    emit enrollProgress(1, QStringLiteral("请将手指按在指纹仪上（第 1 次）"));
    qDebug() << "[FingerprintSource] 轮询已启动（注册模式）";
#else
    emit sourceError(QStringLiteral("指纹模块仅在 Linux 板端可用"));
#endif
}

void FingerprintSource::stop()
{
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
        qDebug() << "[FingerprintSource] 轮询已停止";
    }
    m_enrollMode = false;
    m_enrollState = Idle;
    m_liftConfirmCount = 0;
}

/* ---- 轮询：检测手指 → 提取特征 → 上传模板 ---- */
void FingerprintSource::poll()
{
#ifdef Q_OS_LINUX
    if (!m_deviceOpen || !g_ZAZGetImage) {
        return;
    }

    int ret = g_ZAZGetImage(m_handle, ZAZ_DEV_ADDR);

    if (m_enrollMode) {
        /* ---- 注册模式状态机 ---- */
        switch (m_enrollState) {

        case WaitPress1: {
            if (ret == ZAZ_NO_FINGER) {
                m_pollTimer->start();
                return;
            }
            qDebug() << "[FingerprintSource] 注册：检测到第 1 次按指纹";
            int genRet = g_ZAZGenChar(m_handle, ZAZ_DEV_ADDR, CHAR_BUFFER_A);
            if (genRet != ZAZ_OK) {
                qDebug() << "[FingerprintSource] GenChar(A) 失败，code=" << genRet;
                emit enrollProgress(1, QStringLiteral("按压不清晰，请松开后重新按下（第 1 次）"));
                m_pollTimer->start();
                return;
            }
            m_enrollState = WaitLift;
            m_liftConfirmCount = 0;
            m_stepTimer.restart();
            emit enrollProgress(2, QStringLiteral("✓ 第 1 次采集成功，请松开手指"));
            qDebug() << "[FingerprintSource] 第 1 次采集成功，等待松手";
            m_pollTimer->start();
            return;
        }

        case WaitLift: {
            if (m_stepTimer.elapsed() > 15000) {
                m_enrollState = WaitPress1;
                m_stepTimer.restart();
                emit enrollProgress(1, QStringLiteral("检测超时，请重新按下手指（第 1 次）"));
                m_pollTimer->start();
                return;
            }
            if (ret == ZAZ_NO_FINGER) {
                m_liftConfirmCount++;
                if (m_liftConfirmCount >= 3) {
                    m_enrollState = WaitPress2;
                    m_stepTimer.restart();
                    emit enrollProgress(3, QStringLiteral("请再次按下手指（第 2 次）"));
                    qDebug() << "[FingerprintSource] 松手确认，等待第 2 次按指纹";
                }
            } else {
                m_liftConfirmCount = 0;
            }
            m_pollTimer->start();
            return;
        }

        case WaitPress2: {
            if (m_stepTimer.elapsed() > 30000) {
                m_enrollState = WaitPress1;
                m_stepTimer.restart();
                emit enrollProgress(1, QStringLiteral("超时，请重新开始（第 1 次按下手指）"));
                m_pollTimer->start();
                return;
            }
            if (ret == ZAZ_NO_FINGER) {
                m_pollTimer->start();
                return;
            }
            qDebug() << "[FingerprintSource] 注册：检测到第 2 次按指纹";
            int genRet = g_ZAZGenChar(m_handle, ZAZ_DEV_ADDR, CHAR_BUFFER_B);
            if (genRet != ZAZ_OK) {
                qDebug() << "[FingerprintSource] GenChar(B) 失败，code=" << genRet;
                m_enrollState = WaitLift;
                m_liftConfirmCount = 0;
                m_stepTimer.restart();
                emit enrollProgress(2, QStringLiteral("按压不清晰，请松开后再按（第 2 次）"));
                m_pollTimer->start();
                return;
            }

            /* 第二次采集成功 → 合成 */
            emit enrollProgress(4, QStringLiteral("正在合成指纹模板..."));
            qDebug() << "[FingerprintSource] 第 2 次采集成功，开始合成";

            int mergeRet = g_ZAZRegModule(m_handle, ZAZ_DEV_ADDR);
            if (mergeRet != ZAZ_OK) {
                qDebug() << "[FingerprintSource] RegModule 失败，code=" << mergeRet;
                m_enrollState = WaitPress1;
                m_stepTimer.restart();
                emit enrollProgress(1,
                    QStringLiteral("指纹合成失败（%1），请重新开始（第 1 次按下手指）").arg(mergeRet));
                m_pollTimer->start();
                return;
            }

            /* 合成成功 → 上传模板 */
            g_ZAZSetCharLen(kTemplateSize);
            unsigned char templateBuf[2048] = {};
            int templateLen = 0;
            int upRet = g_ZAZUpChar(m_handle, ZAZ_DEV_ADDR, CHAR_BUFFER_A, templateBuf, &templateLen);
            if (upRet != ZAZ_OK || templateLen <= 0) {
                qDebug() << "[FingerprintSource] UpChar 失败，code=" << upRet << "len=" << templateLen;
                m_enrollState = WaitPress1;
                m_stepTimer.restart();
                emit enrollProgress(1,
                    QStringLiteral("模板读取失败（%1），请重新开始（第 1 次按下手指）").arg(upRet));
                m_pollTimer->start();
                return;
            }

            qDebug() << "[FingerprintSource] 注册采集完成，模板长度=" << templateLen;
            emit enrollProgress(5, QStringLiteral("✓ 指纹采集完成，正在保存..."));

            FingerprintCredential credential;
            credential.tenantId = ConfigManager::instance()->tenantCode().trimmed();
            if (credential.tenantId.isEmpty()) {
                credential.tenantId = QStringLiteral("000000");
            }
            credential.templateData = QByteArray(reinterpret_cast<const char *>(templateBuf), templateLen);
            emit fingerprintCaptured(credential);
            /* 注册完成后不恢复轮询 */
            return;
        }

        default:
            m_pollTimer->start();
            return;
        }
    }

    /* ---- 登录模式：单次采集 ---- */
    if (ret == ZAZ_NO_FINGER) {
        return;
    }
    if (ret != ZAZ_OK) {
        qDebug() << "[FingerprintSource] GetImage 失败，code=" << ret;
        return;
    }

    m_pollTimer->stop();
    qDebug() << "[FingerprintSource] 检测到手指，开始采集";

    ret = g_ZAZGenChar(m_handle, ZAZ_DEV_ADDR, CHAR_BUFFER_A);
    if (ret != ZAZ_OK) {
        qDebug() << "[FingerprintSource] GenChar 失败，code=" << ret;
        m_pollTimer->start();
        return;
    }

    g_ZAZSetCharLen(kTemplateSize);

    unsigned char templateBuf[2048] = {};
    int templateLen = 0;
    ret = g_ZAZUpChar(m_handle, ZAZ_DEV_ADDR, CHAR_BUFFER_A, templateBuf, &templateLen);
    if (ret != ZAZ_OK || templateLen <= 0) {
        qDebug() << "[FingerprintSource] UpChar 失败，code=" << ret << "len=" << templateLen;
        m_pollTimer->start();
        return;
    }

    qDebug() << "[FingerprintSource] 采集成功，模板长度=" << templateLen;

    /* 去重：短时间内同一手指不重复发送 */
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if ((nowMs - m_lastCaptureMs) < kCaptureDedupeMs) {
        qDebug() << "[FingerprintSource] 去重跳过";
        m_pollTimer->start();
        return;
    }
    m_lastCaptureMs = nowMs;

    FingerprintCredential credential;
    credential.tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (credential.tenantId.isEmpty()) {
        credential.tenantId = QStringLiteral("000000");
    }
    credential.templateData = QByteArray(reinterpret_cast<const char *>(templateBuf), templateLen);
    emit fingerprintCaptured(credential);

    m_pollTimer->start();
#endif
}

/* ---- SDK 加载 ---- */
bool FingerprintSource::loadSdk()
{
#ifdef Q_OS_LINUX
    for (int i = 0; kSdkPaths[i]; ++i) {
        m_lib = dlopen(kSdkPaths[i], RTLD_LAZY);
        if (m_lib) {
            qDebug() << "[FingerprintSource] SDK 加载成功:" << kSdkPaths[i];
            break;
        }
    }
    if (!m_lib) {
        const QString err = QStringLiteral("无法加载 libzazapi.so: %1").arg(QString::fromLocal8Bit(dlerror()));
        qDebug() << "[FingerprintSource]" << err;
        emit sourceError(err);
        return false;
    }

    g_ZAZ_MODE        = reinterpret_cast<fn_ZAZ_MODE>(dlsym(m_lib, "ZAZ_MODE"));
    g_ZAZ_SETPATH     = reinterpret_cast<fn_ZAZ_SETPATH>(dlsym(m_lib, "ZAZ_SETPATH"));
    g_ZAZSetlog       = reinterpret_cast<fn_ZAZSetlog>(dlsym(m_lib, "ZAZSetlog"));
    g_ZAZOpenDeviceEx = reinterpret_cast<fn_ZAZOpenDeviceEx>(dlsym(m_lib, "ZAZOpenDeviceEx"));
    g_ZAZCloseDeviceEx= reinterpret_cast<fn_ZAZCloseDeviceEx>(dlsym(m_lib, "ZAZCloseDeviceEx"));
    g_ZAZVfyPwd       = reinterpret_cast<fn_ZAZVfyPwd>(dlsym(m_lib, "ZAZVfyPwd"));
    g_ZAZGetImage     = reinterpret_cast<fn_ZAZGetImage>(dlsym(m_lib, "ZAZGetImage"));
    g_ZAZGenChar      = reinterpret_cast<fn_ZAZGenChar>(dlsym(m_lib, "ZAZGenChar"));
    g_ZAZSetCharLen   = reinterpret_cast<fn_ZAZSetCharLen>(dlsym(m_lib, "ZAZSetCharLen"));
    g_ZAZUpChar       = reinterpret_cast<fn_ZAZUpChar>(dlsym(m_lib, "ZAZUpChar"));
    g_ZAZRegModule    = reinterpret_cast<fn_ZAZRegModule>(dlsym(m_lib, "ZAZRegModule"));

    if (!g_ZAZOpenDeviceEx || !g_ZAZGetImage || !g_ZAZGenChar || !g_ZAZUpChar) {
        const QString err = QStringLiteral("SDK 函数解析失败");
        qDebug() << "[FingerprintSource]" << err;
        emit sourceError(err);
        dlclose(m_lib);
        m_lib = nullptr;
        return false;
    }

    return true;
#else
    return false;
#endif
}

/* ---- 设备初始化（与官方 demo TESTDEV 模式一致） ---- */
bool FingerprintSource::initDevice()
{
#ifdef Q_OS_LINUX
    /* 1. 设置工作模式 */
    if (g_ZAZ_MODE) {
        g_ZAZ_MODE(0);
    }
    if (g_ZAZSetlog) {
        g_ZAZSetlog(0);  /* 0=关日志，1=开日志 */
    }

    /* 2. 设置设备类型（fp_kvm=6, path_pc=1） */
    if (g_ZAZ_SETPATH) {
        g_ZAZ_SETPATH(6, 1);
    }

    /* 3. 打开设备 */
    int ret = g_ZAZOpenDeviceEx(&m_handle, ZAZ_DEVICE_USB, nullptr, 2, 0);
    if (ret != ZAZ_OK) {
        const QString err = QStringLiteral("指纹设备打开失败，code=%1").arg(ret);
        qDebug() << "[FingerprintSource]" << err;
        emit sourceError(err);
        return false;
    }
    qDebug() << "[FingerprintSource] 设备已打开，handle=" << m_handle;

    /* 4. 校验密码 */
    if (g_ZAZVfyPwd) {
        unsigned char pwd[4] = {0, 0, 0, 0};
        ret = g_ZAZVfyPwd(m_handle, ZAZ_DEV_ADDR, pwd);
        if (ret != ZAZ_OK) {
            qDebug() << "[FingerprintSource] 密码校验失败，code=" << ret << "（非致命，继续）";
        }
    }

    m_deviceOpen = true;
    return true;
#else
    return false;
#endif
}

/* ---- 关闭设备 ---- */
void FingerprintSource::closeDevice()
{
#ifdef Q_OS_LINUX
    if (m_deviceOpen && g_ZAZCloseDeviceEx) {
        g_ZAZCloseDeviceEx(m_handle);
        qDebug() << "[FingerprintSource] 设备已关闭";
    }
    m_deviceOpen = false;
    m_handle = -1;
#endif
}
