/**
 * @file zazapi.h
 * @brief ZAZ 指纹 SDK 动态加载函数指针声明
 *
 * 签名来源：官方 SYProtocol.h + D:\Desktop\demo
 * 板端 SDK 路径：/home/linaro/AdapterService/bin/libs/libzazapi.so
 */
#ifndef ZAZAPI_H
#define ZAZAPI_H

/* ---- SDK 原始类型 ---- */
#define ZAZ_HANDLE int

/* ---- 返回码 ---- */
#define ZAZ_OK          0x00
#define ZAZ_NO_FINGER   0x02

/* ---- 缓冲区 ID ---- */
#define CHAR_BUFFER_A   0x01
#define CHAR_BUFFER_B   0x02

/* ---- 设备地址（固定值） ---- */
#define ZAZ_DEV_ADDR    0xFFFFFFFF

/* ---- 设备类型 ---- */
#define ZAZ_DEVICE_USB  0

/* ---- 函数指针类型 ---- */
typedef int (*fn_ZAZ_MODE)(int mode);
typedef int (*fn_ZAZ_SETPATH)(int fptype, int path);
typedef void (*fn_ZAZSetlog)(int val);
typedef int (*fn_ZAZOpenDeviceEx)(ZAZ_HANDLE *hHandle, int nDeviceType,
                                  const char *nPortNum, int nPortPara, int nPackageSize);
typedef int (*fn_ZAZCloseDeviceEx)(ZAZ_HANDLE hHandle);
typedef int (*fn_ZAZVfyPwd)(ZAZ_HANDLE hHandle, int nAddr, unsigned char *pPassword);
typedef int (*fn_ZAZGetImage)(ZAZ_HANDLE hHandle, int nAddr);
typedef int (*fn_ZAZGenChar)(ZAZ_HANDLE hHandle, int nAddr, int iBufferID);
typedef int (*fn_ZAZRegModule)(ZAZ_HANDLE hHandle, int nAddr);
typedef int (*fn_ZAZSetCharLen)(int nLen);
typedef int (*fn_ZAZUpChar)(ZAZ_HANDLE hHandle, int nAddr, int iBufferID,
                            unsigned char *pTemplet, int *iTempletLength);

#endif // ZAZAPI_H
