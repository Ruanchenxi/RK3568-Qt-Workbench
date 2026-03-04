/**
 * @file AuthService.cpp
 * @brief 认证服务实现
 */

#include "../core/authservice.h"
#include "../core/ConfigManager.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

AuthService *AuthService::m_instance = nullptr;

AuthService::AuthService(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
}

AuthService::~AuthService()
{
}

AuthService *AuthService::instance()
{
    if (!m_instance)
    {
        m_instance = new AuthService();
    }
    return m_instance;
}

/**
 * @brief 登录方法（对外接口）
 * @param userName 用户名
 * @param password 密码
 * @param tenantId 租户ID（可选）
 */
void AuthService::login(const QString &userName, const QString &password, const QString &tenantId)
{
    Q_UNUSED(tenantId); // 暂不使用租户ID

    // 输入验证
    if (userName.trimmed().isEmpty())
    {
        emit loginFailed("用户名不能为空");
        return;
    }

    if (password.isEmpty())
    {
        emit loginFailed("密码不能为空");
        return;
    }

    // 加密密码
    QString encryptedPassword = encryptPassword(password);

    // 发送登录请求
    sendLoginRequest(userName, encryptedPassword, tenantId);
}

/**
 * @brief MD5 加密密码
 */
QString AuthService::encryptPassword(const QString &password) const
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5);
    return QString(hash.toHex());
}

/**
 * @brief 发送登录网络请求
 */
void AuthService::sendLoginRequest(const QString &userName, const QString &encryptedPassword, const QString &tenantId)
{
    // ============================================
    // TODO: 实际项目中，这里应该调用后端API
    // 示例：
    // QNetworkRequest request(QUrl(ConfigManager::instance()->apiUrl() + "/auth/login"));
    // request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // QJsonObject json;
    // json["username"] = userName;
    // json["password"] = encryptedPassword;
    // json["tenantId"] = tenantId;
    // QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    // ============================================

    // 模拟验证 - 生产环境请删除此代码并替换为真实API调用
    QString adminHash = encryptPassword("admin123");
    QString operatorHash = encryptPassword("123456");

    QJsonObject userInfo;

    if (userName == "admin" && encryptedPassword == adminHash)
    {
        userInfo["user_id"] = "U001";
        userInfo["user_name"] = userName;
        userInfo["real_name"] = "系统管理员";
        userInfo["role_name"] = "管理员";
        userInfo["department"] = "信息中心";

        saveToken("mock_access_token_admin", "mock_refresh_token_admin", userInfo);
        emit loginSuccess(userInfo);

        qDebug() << "[AuthService] 用户登录成功:" << userName;
    }
    else if (userName == "operator" && encryptedPassword == operatorHash)
    {
        userInfo["user_id"] = "U002";
        userInfo["user_name"] = userName;
        userInfo["real_name"] = "张三";
        userInfo["role_name"] = "操作员";
        userInfo["department"] = "运维部";

        saveToken("mock_access_token_operator", "mock_refresh_token_operator", userInfo);
        emit loginSuccess(userInfo);

        qDebug() << "[AuthService] 用户登录成功:" << userName;
    }
    else if (userName == "reviewer" && encryptedPassword == operatorHash)
    {
        userInfo["user_id"] = "U003";
        userInfo["user_name"] = userName;
        userInfo["real_name"] = "李四";
        userInfo["role_name"] = "审核员";
        userInfo["department"] = "安全部";

        saveToken("mock_access_token_reviewer", "mock_refresh_token_reviewer", userInfo);
        emit loginSuccess(userInfo);

        qDebug() << "[AuthService] 用户登录成功:" << userName;
    }
    else
    {
        emit loginFailed("用户名或密码错误");
        qDebug() << "[AuthService] 用户登录失败:" << userName;
    }
}

/**
 * @brief 保存 token 和用户信息
 */
void AuthService::saveToken(const QString &accessToken, const QString &refreshToken, const QJsonObject &userInfo)
{
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_userInfo = userInfo;
    m_userName = userInfo["user_name"].toString();
}

/**
 * @brief 获取 access token
 */
QString AuthService::getAccessToken() const
{
    return m_accessToken;
}

/**
 * @brief 获取 refresh token
 */
QString AuthService::getRefreshToken() const
{
    return m_refreshToken;
}

/**
 * @brief 获取用户信息
 */
QJsonObject AuthService::getUserInfo() const
{
    return m_userInfo;
}

/**
 * @brief 获取用户名
 */
QString AuthService::getUserName() const
{
    return m_userName;
}

/**
 * @brief 清除 token（退出登录）
 */
void AuthService::clearToken()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_userInfo = QJsonObject();
    m_userName.clear();

    emit loggedOut();
}

/**
 * @brief 检查是否已登录
 */
bool AuthService::isLoggedIn() const
{
    return !m_accessToken.isEmpty();
}

/**
 * @brief 获取认证请求头（用于其他 API 请求）
 */
QString AuthService::getAuthorizationHeader() const
{
    if (m_accessToken.isEmpty())
    {
        return QString();
    }
    return QString("Bearer %1").arg(m_accessToken);
}

QString AuthService::hashPassword(const QString &password, const QString &salt)
{
    // 使用 SHA256 加密密码
    QString saltedPassword = password + salt;
    QByteArray hash = QCryptographicHash::hash(
        saltedPassword.toUtf8(),
        QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

LoginResult AuthService::loginWithPassword(const QString &username, const QString &password)
{
    LoginResult result;

    // 输入验证
    if (username.trimmed().isEmpty())
    {
        result.success = false;
        result.errorMessage = "用户名不能为空";
        emit loginFailed(result.errorMessage);
        return result;
    }

    if (password.isEmpty())
    {
        result.success = false;
        result.errorMessage = "密码不能为空";
        emit loginFailed(result.errorMessage);
        return result;
    }

    // 加密密码
    QString passwordHash = hashPassword(password);

    // 验证凭据
    result = verifyCredentials(username, passwordHash);

    if (result.success)
    {
        // 登录成功，设置全局用户状态
        AppContext::instance()->setCurrentUser(result.userInfo);
        emit loginSucceeded(result.userInfo);

        qDebug() << "[AuthService] 用户登录成功:" << username;
    }
    else
    {
        emit loginFailed(result.errorMessage);
        qDebug() << "[AuthService] 用户登录失败:" << username << result.errorMessage;
    }

    return result;
}

LoginResult AuthService::loginWithCard(const QString &cardId)
{
    LoginResult result;

    if (cardId.isEmpty())
    {
        result.success = false;
        result.errorMessage = "未检测到卡片";
        emit loginFailed(result.errorMessage);
        return result;
    }

    // TODO: 实际项目中调用后端API验证卡号
    // 这里模拟验证
    if (cardId == "CARD001" || cardId == "CARD002")
    {
        result.success = true;
        result.userInfo.userId = "U" + cardId;
        result.userInfo.username = "card_user";
        result.userInfo.realName = "刷卡用户";
        result.userInfo.role = "操作员";
        result.userInfo.department = "运维部";

        AppContext::instance()->setCurrentUser(result.userInfo);
        emit loginSucceeded(result.userInfo);
    }
    else
    {
        result.success = false;
        result.errorMessage = "未识别的卡片";
        emit loginFailed(result.errorMessage);
    }

    return result;
}

LoginResult AuthService::loginWithFingerprint(const QString &fingerprintId)
{
    LoginResult result;

    if (fingerprintId.isEmpty())
    {
        result.success = false;
        result.errorMessage = "未检测到指纹";
        emit loginFailed(result.errorMessage);
        return result;
    }

    // TODO: 实际项目中调用后端API验证指纹
    // 这里模拟验证
    if (fingerprintId.startsWith("FP"))
    {
        result.success = true;
        result.userInfo.userId = "U" + fingerprintId;
        result.userInfo.username = "fp_user";
        result.userInfo.realName = "指纹用户";
        result.userInfo.role = "操作员";
        result.userInfo.department = "运维部";

        AppContext::instance()->setCurrentUser(result.userInfo);
        emit loginSucceeded(result.userInfo);
    }
    else
    {
        result.success = false;
        result.errorMessage = "指纹验证失败";
        emit loginFailed(result.errorMessage);
    }

    return result;
}

void AuthService::logout()
{
    QString username = AppContext::instance()->currentUser().username;
    AppContext::instance()->clearCurrentUser();
    emit loggedOut();

    qDebug() << "[AuthService] 用户登出:" << username;
}

bool AuthService::changePassword(const QString &oldPassword, const QString &newPassword)
{
    // 验证旧密码
    QString oldHash = hashPassword(oldPassword);

    // TODO: 调用后端API验证旧密码并更新新密码

    // 验证新密码强度
    QString error = validatePasswordStrength(newPassword);
    if (!error.isEmpty())
    {
        return false;
    }

    // 这里模拟成功
    qDebug() << "[AuthService] 密码修改成功";
    return true;
}

QString AuthService::validatePasswordStrength(const QString &password)
{
    if (password.length() < 6)
    {
        return "密码长度不能少于6位";
    }

    if (password.length() > 20)
    {
        return "密码长度不能超过20位";
    }

    // 检查是否包含数字
    bool hasDigit = false;
    bool hasLetter = false;
    for (const QChar &c : password)
    {
        if (c.isDigit())
            hasDigit = true;
        if (c.isLetter())
            hasLetter = true;
    }

    if (!hasDigit || !hasLetter)
    {
        return "密码必须包含字母和数字";
    }

    return QString(); // 空表示通过
}

bool AuthService::needRelogin() const
{
    return AppContext::instance()->isSessionExpired();
}

LoginResult AuthService::verifyCredentials(const QString &username, const QString &passwordHash)
{
    LoginResult result;

    // ============================================
    // TODO: 实际项目中，这里应该调用后端API
    // 示例：
    // QNetworkReply *reply = m_networkManager->post(
    //     QNetworkRequest(QUrl(ConfigManager::instance()->apiUrl() + "/auth/login")),
    //     jsonData
    // );
    // ============================================

    // 模拟验证 - 生产环境请删除此代码并替换为真实API调用
    QString adminHash = hashPassword("admin123");
    QString operatorHash = hashPassword("123456");

    if (username == "admin" && passwordHash == adminHash)
    {
        result.success = true;
        result.userInfo.userId = "U001";
        result.userInfo.username = username;
        result.userInfo.realName = "系统管理员";
        result.userInfo.role = "管理员";
        result.userInfo.department = "信息中心";
    }
    else if (username == "operator" && passwordHash == operatorHash)
    {
        result.success = true;
        result.userInfo.userId = "U002";
        result.userInfo.username = username;
        result.userInfo.realName = "张三";
        result.userInfo.role = "操作员";
        result.userInfo.department = "运维部";
    }
    else if (username == "reviewer" && passwordHash == operatorHash)
    {
        result.success = true;
        result.userInfo.userId = "U003";
        result.userInfo.username = username;
        result.userInfo.realName = "李四";
        result.userInfo.role = "审核员";
        result.userInfo.department = "安全部";
    }
    else
    {
        result.success = false;
        result.errorMessage = "用户名或密码错误";
    }

    return result;
}

UserInfo AuthService::fetchUserInfo(const QString &userId)
{
    UserInfo info;
    // TODO: 从服务器获取用户详细信息
    return info;
}
