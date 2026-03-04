# CODEMAP（核心文件职责索引）

Status: Active  
Last Updated: 2026-03-02  
范围：`src/` 内核心模块与关键文件。  

说明：

1. 本文档不重复目录树细节，目录树请看 `CURRENT_FOLDER_LAYOUT.md`。  
2. 本文档聚焦“这个文件干什么、可依赖什么、不该依赖什么”。  
3. `ui/*.ui` 不在 `src/` 内，按冻结决策保持在项目根 `ui/` 目录。  

本文件的使用方式：

1. 先看「常见改动定位」，快速找到入口。  
2. 再看对应模块文件表，确认依赖边界。  
3. 修改完成后回到 `DEPENDENCY_RULES.md` 做红线自检。  

## 0. 串口端口红线（必须遵守）

1. `/dev/ttyS4` 仅用于钥匙协议链路（`features/key/*`）。  
2. `/dev/ttyS3` 仅用于刷卡采集链路（`features/auth/infra/device/*`）。  
3. 两条链路只共享 `platform/serial/ISerialTransport` 抽象，不共享协议类。  

## 1. app（入口与装配）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/app/main.cpp` | 程序入口、创建主窗口 | 不承载业务逻辑 |
| `src/app/mainwindow.h/.cpp` | 主界面容器、页面承载与信号转发 | 不直接触达串口/网络/进程实现 |
| `src/app/MainWindowController.h/.cpp` | 主窗口级流程编排（页面切换、登录态驱动） | 通过抽象服务编排，不直连底层实现 |
| `src/app/app.pri` | app 模块编译清单 | 仅构建用途 |

## 2. core（通用核心）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/core/AppContext.h/.cpp` | 应用级上下文与装配点 | 页面层禁止直接 include（见依赖红线） |
| `src/core/ConfigManager.h/.cpp` | 配置读写与运行参数管理 | 通过服务/控制器使用，不在页面里做底层读写 |
| `src/core/core.pri` | core 模块编译清单 | 仅构建用途 |

## 3. features/auth（认证域）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/features/auth/ui/loginpage.h/.cpp` | 登录页面展示与交互 | 不直连 QNetwork/QSerial，不直接调设备实现 |
| `src/features/auth/application/LoginController.h/.cpp` | 登录操作编排、页面交互桥接 | 不直接做底层 IO |
| `src/features/auth/application/AuthFlowCoordinator.h/.cpp` | 认证流程状态编排（当前启用密码分支） | 仅依赖认证抽象 |
| `src/features/auth/domain/AuthTypes.h` | 认证领域类型定义 | 纯类型，无 IO 依赖 |
| `src/features/auth/domain/ports/IAuthGateway.h` | 登录 API 抽象 | 上层仅依赖接口 |
| `src/features/auth/domain/ports/ICredentialSource.h` | 采集源抽象（刷卡/指纹/Mock） | 上层仅依赖接口 |
| `src/features/auth/infra/http/AuthGatewayAdapter.h/.cpp` | 对接 HTTP 登录接口 | 不向上泄漏协议细节 |
| `src/features/auth/infra/password/authservice.h/.cpp` | 密码登录实现 | 属于 infra，禁止页面直接依赖 |
| `src/features/auth/infra/password/AuthServiceAdapter.h/.cpp` | 认证实现适配层 | 向 application 提供稳定入口 |
| `src/features/auth/infra/device/MockCredentialSource.h/.cpp` | 测试用采集源 | 仅测试/占位用途 |
| `src/features/auth/infra/device/CardSerialSource.h/.cpp` | 刷卡采集源占位 | 后续接 `/dev/ttyS3` |
| `src/features/auth/infra/device/FingerprintSource.h/.cpp` | 指纹采集源占位 | 后续接设备 SDK |

## 4. features/key（钥匙与串口报文域）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/features/key/ui/keymanagepage.h/.cpp` | 钥匙管理页面与日志表渲染 | 不直接 include 具体串口实现 |
| `src/features/key/application/KeyManageController.h/.cpp` | 页面操作编排、会话驱动 | 通过会话服务抽象访问协议能力 |
| `src/features/key/application/KeySessionService.h/.cpp` | 串口会话服务、重试/回放接入点 | 调用 protocol + transport 抽象 |
| `src/features/key/application/SerialLogManager.h/.cpp` | 串口日志缓冲/过滤/导出组织 | 不承载协议帧解析 |
| `src/features/key/protocol/KeySerialClient.h/.cpp` | 串口协议核心状态机与报文处理 | 仅允许依赖 `ISerialTransport` 抽象 |
| `src/features/key/protocol/KeyProtocolDefs.h` | 协议命令与常量定义 | 语义锁定，谨慎改动 |
| `src/features/key/protocol/KeyCrc16.h` | CRC 计算工具 | 协议关键算法，不随意改 |
| `src/features/key/protocol/LogItem.h` | 串口日志数据结构 | 供 UI/Controller 渲染使用 |

## 5. features/system

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/features/system/ui/systempage.h/.cpp` | 系统设置页面展示与操作入口 | 不直接调用底层硬件实现 |
| `src/features/system/application/SystemController.h/.cpp` | 系统设置业务编排 | 通过服务/配置层交互；`availableSerialPorts()` 直接调用 `QSerialPortInfo`（平台枚举，见 DEPENDENCY_RULES §2.6） |

## 6. features/log

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/features/log/ui/logpage.h/.cpp` | 服务日志页面展示 | 页面层不直接持有底层 logger 实现 |
| `src/features/log/application/LogController.h/.cpp` | 日志页面数据编排 | 通过 logging 抽象访问日志数据 |

## 7. features/workbench

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/features/workbench/ui/workbenchpage.h/.cpp` | 工作台页面展示 | 页面层不做网络细节处理 |
| `src/features/workbench/application/WorkbenchController.h/.cpp` | 工作台流程编排 | 通过服务层访问接口能力 |

## 8. platform（基础设施实现）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/platform/serial/ISerialTransport.h` | 串口传输抽象接口 | 协议层可依赖 |
| `src/platform/serial/QtSerialTransport.h/.cpp` | 真机串口实现 | 协议层禁止直接 include |
| `src/platform/serial/ReplaySerialTransport.h/.cpp` | 回放串口实现（无硬件验证） | 协议层禁止直接 include |
| `src/platform/process/ProcessService.h/.cpp` | 进程相关平台能力 | 页面层禁止直接依赖 |
| `src/platform/logging/LogService.h/.cpp` | 日志服务实现 | 页面层通过 controller/adapter 间接使用 |
| `src/platform/platform.pri` | platform 模块编译清单 | 仅构建用途 |

## 9. shared（跨 feature 共享契约）

| 文件 | 职责 | 依赖边界 |
|---|---|---|
| `src/shared/contracts/IAuthService.h` | 认证服务抽象契约 | UI/application 面向抽象编程 |
| `src/shared/contracts/IKeySessionService.h` | 钥匙会话服务抽象契约 | UI/application 面向抽象编程 |
| `src/shared/contracts/IProcessService.h` | 进程服务抽象契约 | 避免页面直连进程实现 |
| `src/shared/contracts/KeyTaskDto.h` | 钥匙任务 DTO | 作为跨层数据载体 |
| `src/shared/contracts/SystemSettingsDto.h` | 系统设置 DTO | 作为跨层数据载体 |
| `src/shared/shared.pri` | shared 模块编译清单 | 仅构建用途 |

## 10. _archive（停用代码）

| 文件/目录 | 职责 | 依赖边界 |
|---|---|---|
| `src/_archive/README.md` | 归档区说明 | 主链路禁止依赖 |
| `src/_archive/services/*` | 历史服务实现（停编译） | 仅追溯与对比 |
| `src/_archive/protocol/*` | 历史协议实现（停编译） | 仅追溯与对比 |

## 11. 常见改动定位（任务 -> 目录/文件）

| 常见任务 | 主要修改位置 | 备注 |
|---|---|---|
| 新增一个登录方式（先不接硬件） | `features/auth/domain/*`, `features/auth/application/*`, `features/auth/infra/http/*` | 先走 `IAuthGateway + AuthFlowCoordinator`，不要把协议字段放到 UI |
| 接入刷卡串口采集 | `features/auth/infra/device/CardSerialSource.*`, `platform/serial/*` | 严格使用 `/dev/ttyS3`，不要并入钥匙会话 |
| 新增钥匙协议报文 | `features/key/protocol/*`, `features/key/application/KeySessionService.*` | 协议语义锁定区，改动需最小且可回放验证 |
| 调整钥匙管理串口日志页 | `features/key/ui/keymanagepage.*`, `features/key/application/SerialLogManager.*` | UI 只做渲染和交互，不下沉协议细节 |
| 调整工作台接口接入 | `features/workbench/application/*`, `features/workbench/ui/*` | 页面不直接写网络细节 |
| 新增系统配置项 | `features/system/application/*`, `features/system/ui/*`, `core/ConfigManager.*` | 配置读写集中到 ConfigManager |

## 12. 维护规则

1. 新增核心文件时，需在本文件补一行“文件-职责-依赖边界”。  
2. 文件职责应与该文件头注释一致（见 `docs/COMMENTING_GUIDE.md`）。  
3. 若目录结构变化，先更新 `CURRENT_FOLDER_LAYOUT.md`，再更新本文件。  

## 13. 典型调用链（阅读代码建议顺序）

### 13.1 登录链路（当前密码分支）

1. `features/auth/ui/loginpage.*`（采集输入）  
2. `features/auth/application/LoginController.*`（触发流程）  
3. `features/auth/application/AuthFlowCoordinator.*`（模式分派）  
4. `features/auth/domain/ports/IAuthGateway.h`（抽象）  
5. `features/auth/infra/http/AuthGatewayAdapter.*`（实现）  

### 13.2 钥匙串口链路

1. `features/key/ui/keymanagepage.*`（触发与展示）  
2. `features/key/application/KeyManageController.*`（页面编排）  
3. `features/key/application/KeySessionService.*`（会话编排）  
4. `features/key/protocol/KeySerialClient.*`（协议语义核心）  
5. `platform/serial/ISerialTransport.h`（抽象）  
6. `platform/serial/QtSerialTransport.*` / `ReplaySerialTransport.*`（实现）  

## 14. 修改时常见误区（避免回退）

1. 在 `ui` 层直接 include `QSerialPort/QNetworkAccessManager`。  
2. 在 `protocol` 层直接 include `QtSerialTransport/ReplaySerialTransport`。  
3. 将单 feature 临时 DTO 放进 `shared/contracts`。  
4. 从 `src/_archive` 复制回主链路而不走评审与验证。  
