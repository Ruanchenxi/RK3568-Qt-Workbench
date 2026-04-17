/**
 * @file SystemIdentityUserDto.h
 * @brief 用户身份采集页使用的用户条目 DTO
 */
#ifndef SYSTEMIDENTITYUSERDTO_H
#define SYSTEMIDENTITYUSERDTO_H

#include <QString>

struct SystemIdentityUserDto
{
    QString userId;
    QString account;
    QString realName;
    QString cardNo;
    QString fingerprint;
    QString roleName;

    bool isValid() const
    {
        return !account.trimmed().isEmpty() || !userId.trimmed().isEmpty();
    }

    bool canUpdateCardNo() const
    {
        return !userId.trimmed().isEmpty();
    }
};

#endif // SYSTEMIDENTITYUSERDTO_H
