# 当前目录结构说明（已落地）

Status: Active  
Owner: 架构维护  
Last Updated: 2026-03-02  
适用范围：描述当前真实代码目录、每个文件夹用途、每个核心 `.h/.cpp` 文件职责。  
不适用范围：不描述未来目标结构（见 `TARGET_FOLDER_LAYOUT.md`）。  

---

## 1. 怎么使用这份文档

你可以按下面顺序看：

1. 先看第 2 节，了解当前真实目录树。  
2. 再看第 3 节，确认每一层目录边界。  
3. 然后看第 4 节，按模块查看“每个文件在干什么”。  
4. 最后看第 5 节，按“要做的改动”反查应该改哪个目录和文件。  

这份文档是“落地手册”，目标是让新同事或零历史 agent 不需要猜文件位置。

---

## 2. 当前 `src/` 目录树（真实）

```text
src/
├─ app/
├─ core/
├─ features/
│  ├─ auth/
│  ├─ key/
│  ├─ log/
│  ├─ system/
│  └─ workbench/
├─ platform/
│  ├─ logging/
│  ├─ process/
│  └─ serial/
├─ shared/
│  └─ contracts/
└─ _archive/
```

上面是“一级结构”。实际文件职责见第 4 节。

---

## 3. 每个顶层文件夹是干什么的

### 3.1 `src/app/`

定位：程序入口层和主窗口编排层。  
应该放：`main.cpp`、主窗口容器、页面切换控制。  
不应该放：串口协议解析、HTTP 细节、设备访问细节。  

### 3.2 `src/core/`

定位：应用级通用核心能力（少量、稳定）。  
应该放：应用上下文装配、全局配置管理。  
不应该放：任何 feature 专属业务逻辑。  

### 3.3 `src/features/`

定位：业务主战场，按领域拆分。  
当前有：`auth` / `key` / `system` / `log` / `workbench`。  
分层规则：每个 feature 内部按 `ui`、`application`、`domain`、`infra` 分层（按实际需要出现）。  

### 3.4 `src/platform/`

定位：可替换基础设施实现层。  
应该放：串口实现、进程实现、日志实现。  
不应该放：页面行为、业务状态机。  

### 3.5 `src/shared/contracts/`

定位：跨 feature 的稳定接口和 DTO。  
应该放：被多个 feature 复用且已稳定的契约。  
不应该放：某一个 feature 临时使用、未来不稳定的结构。  

### 3.6 `src/_archive/`

定位：停用/历史代码归档区。  
用途：追溯、比对、应急回滚参考。  
强约束：主链路禁止依赖，禁止 include。  

---

## 4. 文件级职责说明（`.h/.cpp` 逐项）

下面按目录列出当前核心文件，说明“这个文件干嘛、改什么问题该动它”。

### 4.1 `src/app/`

#### `main.cpp`

1. Qt 应用入口。  
2. 创建主窗口并启动事件循环。  
3. 出现“程序启动初始化顺序”问题时改这里。  

#### `mainwindow.h/.cpp`

1. 主界面容器，承载各业务页面。  
2. 处理页面切换、顶层 UI 信号连接。  
3. 不应承载业务规则（业务交给 controller/service）。  

#### `MainWindowController.h/.cpp`

1. 主窗口级编排逻辑（登录后跳转、模块入口调度）。  
2. 连接页面事件与应用层动作。  
3. 不直接操作串口/网络底层实现。  

#### `app.pri`

1. qmake 编译清单。  
2. 新增 app 层源文件时需要同步更新。  

---

### 4.2 `src/core/`

#### `AppContext.h/.cpp`

1. 应用依赖装配点（把具体实现注入到抽象接口）。  
2. 集中管理跨模块对象生命周期。  
3. 出现“依赖实例在哪创建”问题时先看这里。  

#### `ConfigManager.h/.cpp`

1. 系统配置读取/写入与缓存。  
2. 管理如站号、接口地址、串口参数等配置数据。  
3. 系统设置页保存失败、配置不生效时应优先排查这里。  

#### `core.pri`

1. core 模块编译清单。  

---

### 4.3 `src/features/auth/`

#### `ui/loginpage.h/.cpp`

1. 登录页面展示与用户输入采集。  
2. 触发登录动作、显示结果、处理页面级交互。  
3. 不应直接调用 `QNetworkAccessManager` 或串口实现。  

#### `application/LoginController.h/.cpp`

1. 登录页面与认证流程之间的编排层。  
2. 接收页面输入，组织调用流程，回传可渲染结果。  

#### `application/AuthFlowCoordinator.h/.cpp`

1. 认证流程协调器（按登录模式分支：密码/刷卡/指纹）。  
2. 当前可先仅启用密码分支，刷卡/指纹分支作为扩展位。  

#### `domain/AuthTypes.h`

1. 认证领域类型定义（请求参数、结果模型等）。  
2. 纯数据/类型层，不做 IO。  

#### `domain/ports/IAuthGateway.h`

1. 认证 API 抽象接口。  
2. 上层通过它发起登录，不关心 HTTP 细节。  

#### `domain/ports/ICredentialSource.h`

1. 认证凭据采集抽象接口。  
2. 统一抽象刷卡源、指纹源、Mock 源。  

#### `infra/http/AuthGatewayAdapter.h/.cpp`

1. `IAuthGateway` 的 HTTP 实现。  
2. 处理 URL、header、字段映射、响应解析。  
3. 后端字段变化时优先改这里，不要污染 UI/Application。  

#### `infra/password/authservice.h/.cpp`

1. 密码认证具体实现。  
2. 保持现有账号密码能力可用。  

#### `infra/password/AuthServiceAdapter.h/.cpp`

1. 对旧认证实现做适配，向上提供稳定接口。  
2. 用于迁移阶段降低改动面。  

#### `infra/device/MockCredentialSource.h/.cpp`

1. 测试用凭据源（人工触发，不建议自动循环触发）。  
2. 用于无硬件阶段流程打通。  

#### `infra/device/CardSerialSource.h/.cpp`

1. 刷卡采集源占位实现。  
2. 未来接入 `/dev/ttyS3`。  

#### `infra/device/FingerprintSource.h/.cpp`

1. 指纹采集源占位实现。  
2. 未来接入指纹设备 SDK/驱动。  

---

### 4.4 `src/features/key/`

#### `ui/keymanagepage.h/.cpp`

1. 钥匙管理页 UI 展示与交互。  
2. 展示串口报文日志表、任务列表、操作结果。  
3. 不直接实现协议逻辑。  

#### `application/KeyManageController.h/.cpp`

1. 钥匙管理业务编排层。  
2. 接收页面操作（查询、删除等），调度会话服务并回传页面状态。  

#### `application/KeySessionService.h/.cpp`

1. 串口会话服务核心入口。  
2. 管理命令发送、重试、超时、回放/真机切换。  

#### `application/SerialLogManager.h/.cpp`

1. 串口日志行模型管理与导出。  
2. 处理日志过滤、摘要、HEX 展示数据。  

#### `protocol/KeySerialClient.h/.cpp`

1. 钥匙协议核心逻辑（帧处理、状态机、握手语义）。  
2. 协议语义锁定区，禁止随意改行为。  
3. 只允许依赖 `ISerialTransport` 抽象。  

#### `protocol/KeyProtocolDefs.h`

1. 协议常量、命令定义、字段约束。  

#### `protocol/KeyCrc16.h`

1. CRC16 校验算法。  
2. 属于协议兼容关键点，改动需严格验证。  

#### `protocol/LogItem.h`

1. 串口日志结构定义（时间、方向、命令、HEX 等）。  
2. 供 UI 层渲染表格。  

---

### 4.5 `src/features/system/`

#### `ui/systempage.h/.cpp`

1. 系统设置页面 UI。  
2. 展示并编辑站号、租户编码、串口参数等。  

#### `application/SystemController.h/.cpp`

1. 系统设置业务编排。  
2. 负责页面数据与 `ConfigManager` 之间的数据流。  

---

### 4.6 `src/features/log/`

#### `ui/logpage.h/.cpp`

1. 服务日志页面 UI。  
2. 负责日志展示和用户查询交互。  

#### `application/LogController.h/.cpp`

1. 日志页面业务编排。  
2. 对接日志服务并输出可渲染数据。  

---

### 4.7 `src/features/workbench/`

#### `ui/workbenchpage.h/.cpp`

1. 工作台页面 UI 容器。  
2. 承载业务主视图或嵌入页面。  

#### `application/WorkbenchController.h/.cpp`

1. 工作台业务编排。  
2. 管理工作台初始化、刷新、入口行为。  

---

### 4.8 `src/platform/serial/`

#### `ISerialTransport.h`

1. 串口传输抽象接口。  
2. 协议层只依赖这个接口，不依赖具体实现。  

#### `QtSerialTransport.h/.cpp`

1. 真机串口实现（基于 Qt 串口）。  
2. 负责真实设备收发。  

#### `ReplaySerialTransport.h/.cpp`

1. 回放串口实现。  
2. 用 JSONL 脚本模拟收发与异常，支持无硬件闭环验证。  

---

### 4.9 `src/platform/process/`

#### `ProcessService.h/.cpp`

1. 进程相关平台能力封装。  
2. 避免页面层直接操作系统进程 API。  

---

### 4.10 `src/platform/logging/`

#### `LogService.h/.cpp`

1. 平台日志能力封装。  
2. 提供日志写入/读取基础能力。  

---

### 4.11 `src/shared/contracts/`

#### `IAuthService.h`

1. 认证服务契约接口。  
2. 上层编排只依赖抽象，不依赖具体实现。  

#### `IKeySessionService.h`

1. 钥匙会话契约接口。  
2. 页面/controller 通过该接口发起钥匙命令。  

#### `IProcessService.h`

1. 进程服务契约接口。  

#### `KeyTaskDto.h`

1. 钥匙任务数据传输对象。  

#### `SystemSettingsDto.h`

1. 系统设置数据传输对象。  

#### `shared.pri`

1. shared 模块编译清单。  

---

### 4.12 `src/_archive/`

#### `_archive/README.md`

1. 归档区说明。  

#### `_archive/services/*`、`_archive/protocol/*`

1. 历史停用实现，仅用于回溯。  
2. 主链路禁止 include 和依赖。  

---

## 5. 常见改动应该改哪里（快速定位）

1. 新增登录方式（暂不接硬件）：`features/auth/domain` + `features/auth/application` + `features/auth/infra/http`。  
2. 接入刷卡串口：`features/auth/infra/device/CardSerialSource.*`，仅使用 `platform/serial` 抽象。  
3. 新增钥匙协议报文：`features/key/protocol/*` + `features/key/application/KeySessionService.*`。  
4. 优化串口报文表格显示：`features/key/ui/keymanagepage.*` + `features/key/application/SerialLogManager.*`。  
5. 增加系统配置项：`features/system/application/*` + `core/ConfigManager.*`。  

---

## 6. 当前强约束（必须遵守）

1. 继续使用 qmake，暂不迁移 CMake。  
2. `.ui` 文件保持在根目录 `ui/`，不迁移。  
3. 协议语义（7E6C/CRC/重试/稳定窗）不在结构调整中改动。  
4. 页面层禁止直连具体串口/网络/进程实现。  
5. 每次结构调整后必须执行：`.\tools\arch_guard.ps1 -Phase 3`。  

---

## 7. 禁止回流的旧目录入口

以下旧平铺目录如再次成为主链路入口，视为架构回退：

1. `src/services/*`  
2. `src/controllers/*`  
3. `src/protocol/*`  
4. `src/contracts/*`  
5. `src/infra/*`  

---

## 8. 何时必须更新本文件

出现任一情况必须更新：

1. 新增/删除 `src` 一级目录。  
2. 某个 feature 新增关键子层（`domain`/`infra`/`application`/`ui`）。  
3. 新增核心 `.h/.cpp`，且会成为后续开发入口。  
4. 编译清单 `.pri` 组织方式变化。  

---

## 9. 关联文档

1. 目标结构：`TARGET_FOLDER_LAYOUT.md`  
2. 文件职责索引：`CODEMAP.md`  
3. 依赖红线与门禁：`DEPENDENCY_RULES.md`  
4. 迁移历史：`MIGRATION_BATCH_PLAN.md`  
5. 注释规范：`../COMMENTING_GUIDE.md`  
