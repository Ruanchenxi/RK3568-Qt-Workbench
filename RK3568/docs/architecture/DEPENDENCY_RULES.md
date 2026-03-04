# 页面层依赖红线（可执行门禁）

Status: Active  
Owner: 架构维护  
Last Updated: 2026-03-02  
关联脚本：`tools/arch_guard.ps1`

## 1. 文档目标

本文件用于明确“什么依赖是禁止的”，并提供可执行门禁与修复路径。  
核心原则：解耦不依赖人工记忆，依赖脚本规则持续约束。

## 2. 规则总览（必须遵守）

### 2.1 页面层禁止直连底层实现

适用范围：`src/**/*page*.h|cpp` 与 `src/**/mainwindow.h|cpp`

禁止出现：

1. `#include <QSerialPort>`  
2. `#include <QNetworkAccessManager>`  
3. `#include <QProcess>`  
4. `#include "core/AppContext.h"`  
5. `AppContext::instance(...)`  
6. `#include "core/authservice.h"`  
7. `#include "features/auth/infra/password/authservice.h"`  
8. `AuthService::instance(...)`  

正确做法：

1. 页面只依赖 controller/application。  
2. controller 再依赖 shared/contracts 抽象。  
3. 具体实现放在 feature/infra 或 platform。  

### 2.2 `keymanagepage` 禁止直连协议核心（Phase 3 起）

1. Phase 1~2：`keymanagepage.cpp` 出现 `KeySerialClient` 给 warning。  
2. Phase 3 起：视为违规并返回非 0 退出码。  

目的：UI 层只能通过 `KeyManageController` 与会话服务交互，不得直接触碰协议类。

### 2.3 mainwindow 同步受控

1. 禁止 `mainwindow.*` 直连 `AppContext` 单例与认证实现头。  
2. 必须通过 `MainWindowController + IAuthService(+Adapter)` 间接访问认证能力。  

### 2.4 协议层禁止依赖具体串口实现

适用范围：`src/**/protocol/*.h|cpp`

禁止出现：

1. `#include "platform/serial/QtSerialTransport.h"`  
2. `#include "platform/serial/ReplaySerialTransport.h"`  

允许依赖：

1. `platform/serial/ISerialTransport.h` 抽象接口。  

### 2.6 SystemController 直接调用 `QSerialPortInfo`（接受的例外）

适用范围：`src/features/system/application/SystemController.h/.cpp`

**例外说明**：`SystemController::availableSerialPorts()` 直接调用 `QSerialPortInfo::availablePorts()`，
而不是通过独立的 `SerialPortEnumerator` 服务。  

**理由**：

1. 枚举串口是纯平台查询，无副作用，不需要 mock。  
2. 抽取单独接口会引入过度设计（单开发者项目）。  
3. **禁止** 将此例外扩展到 UI 层（`*page*.cpp`）—— arch_guard 规则已覆盖。  

### 2.5 禁止回退到旧平铺 include 路径

适用范围：全量源码（排除 `src/_archive`）

禁止出现：

1. `#include "services/..."`  
2. `#include "controllers/..."`  
3. `#include "protocol/..."`（旧平铺路径）  
4. `#include "contracts/..."`（旧平铺路径）  
5. `#include "infra/..."`（旧平铺路径）  

## 3. 执行命令

Phase 1（宽松阶段）：

```powershell
.\tools\arch_guard.ps1 -Phase 1
```

Phase 3（当前默认）：

```powershell
.\tools\arch_guard.ps1 -Phase 3
```

## 4. 违规后的标准处理流程

1. 先看报错定位到文件与 include/调用点。  
2. 判断该依赖是否应上移到 controller/application。  
3. 把具体实现替换为抽象接口依赖。  
4. 必要时新增 adapter/port，不在页面层“临时绕过”。  
5. 重新执行 `arch_guard` 直到 `No violations.`。  

## 5. 常见违规示例与修复

### 示例 A：页面里 include `QSerialPort`

错误：

1. 在 `keymanagepage.cpp` 里直接创建串口对象。  

修复：

1. 页面仅发起“连接串口”意图到 controller。  
2. controller 调用 `IKeySessionService`。  
3. 具体串口实现留在 `platform/serial`。  

### 示例 B：协议层 include `QtSerialTransport.h`

错误：

1. `KeySerialClient.cpp` 直接依赖 Qt 串口实现类。  

修复：

1. 改为依赖 `ISerialTransport` 抽象。  
2. 在 application/service 层注入 QtSerial 或 Replay 实现。  

## 6. 规则设计意图

1. UI 层聚焦渲染与交互，避免底层实现泄漏。  
2. 协议层聚焦语义，避免平台实现耦合。  
3. 依赖方向固定后，回归范围更可控。  
4. 真机/回放切换由装配层完成，业务代码无需感知。  

## 7. 规则变更流程

若需要新增/放宽规则，必须同时完成：

1. 更新本文件。  
2. 更新 `tools/arch_guard.ps1` 对应扫描逻辑。  
3. 在 PR/变更说明写明“为什么必须变更规则”。  
