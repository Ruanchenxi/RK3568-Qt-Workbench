# 串口报文开发手册（零历史 Agent 专用）

> 📋 逐字节协议参考（帧格式/CRC/各命令完整拆解）→ [`PROTOCOL_FRAMES_REFERENCE.md`](./PROTOCOL_FRAMES_REFERENCE.md)

Status: Active  
Owner: 钥匙链路维护者  
Last Updated: 2026-03-11  
适用范围：`features/key` 串口协议链路（UI -> Controller -> Session -> Protocol -> Transport）。  

---

## 0. 信息来源与可信度（先读）

你提供的 `tmp_doc/文档.txt`、`tmp_doc/新建文本文档.txt` 属于**历史阶段文档**，价值很高，但不是“和当前代码 100% 同步”的最终事实。  
因此本手册采用以下优先级：

1. **P0（最高）**：当前分支源码行为（`KeyProtocolDefs` / `KeySerialClient` / `KeySessionService`）。  
2. **P1（高）**：当前版本真实导出日志与回归样本。  
3. **P2（参考）**：历史文档中的协议说明与抓包结论（即你给的 tmp 文档）。  

执行原则：

1. 历史文档用于“理解协议组成”和“补充背景”。  
2. 若历史文档与当前代码冲突，**以当前代码和当前日志为准**。  
3. 若要采用历史文档结论，必须先用当前链路跑一条最小回归验证（SET_COM/Q_TASK/DEL）。  

---

## 1. 你先要知道的 5 件事

1. 串口报文主链路只在 `features/key/*` 和 `platform/serial/*`。  
2. 协议核心在 `src/features/key/protocol/KeySerialClient.*`，这是语义锁定区。  
3. 页面层 `keymanagepage` 只做展示和交互，不直接写协议细节。  
4. 新命令接入必须走统一链路：`UI -> KeyManageController -> IKeySessionService -> KeySerialClient`。  
5. 每次改动后至少执行：编译 + `.\tools\arch_guard.ps1 -Phase 3` + 关键链路回归。  

---

## 2. 串口报文模块地图（改代码先定位）

### 2.1 协议核心（不要乱动）

1. `src/features/key/protocol/KeySerialClient.h/.cpp`  
作用：帧构建、拆包、CRC 校验、重试、超时、KEY_EVENT 稳定性、Q_TASK/DEL 业务语义。  

2. `src/features/key/protocol/KeyProtocolDefs.h`  
作用：协议常量（帧头、命令码、Addr2、超时重试、NAK 错误码）。  

3. `src/features/key/protocol/KeyCrc16.h`  
作用：CRC16 算法。  

4. `src/features/key/protocol/LogItem.h`  
作用：结构化日志数据模型（时间/方向/命令/长度/摘要/Hex/opId）。  

5. `src/features/key/protocol/TicketPayloadEncoder.*`  
作用：传票 `JSON -> payload` 纯协议逻辑。  

6. `src/features/key/protocol/TicketFrameBuilder.*`  
作用：传票 `payload -> frame(s)`、分帧与 CRC。  

7. `src/features/key/protocol/InitPayloadEncoder.*`
作用：初始化接口 JSON -> `INIT(0x02)` payload 纯编码逻辑。  

8. `src/features/key/protocol/RfidPayloadEncoder.*`
作用：RFID 接口 JSON -> `DN_RFID(0x1A)` payload 纯编码逻辑。  

9. `src/features/key/protocol/KeyDataFrameBuilder.*`
作用：`INIT / DN_RFID` 的外层分帧与 CRC。  

### 2.2 编排层（常改）

1. `src/features/key/application/KeySessionService.*`  
作用：会话入口、真机/回放传输注入、事件转发。  

2. `src/features/key/application/KeyManageController.*`  
作用：接收页面操作，发命令，汇总状态与日志。  

3. `src/features/key/application/SerialLogManager.*`  
作用：日志缓存、过滤（专家模式）、CSV 导出。  

4. `src/features/key/application/TicketIngressService.*`  
作用：工作台 JSON 的本地 HTTP 输入源。  

5. `src/features/key/application/TicketStore.*`  
作用：系统票入池、状态管理、原始 JSON 索引。  

6. `src/features/key/application/KeyProvisioningService.*`
作用：初始化 / 下载 RFID 的后端取数、认证头附带与日志输出。  

### 2.3 UI 层（展示与交互）

1. `src/features/key/ui/keymanagepage.*`  
作用：串口报文表格渲染、专家模式开关、Hex 显示开关、导出与清空。  

### 2.4 传输层

1. `src/platform/serial/ISerialTransport.h`（抽象）  
2. `src/platform/serial/QtSerialTransport.*`（真机串口）  
3. `src/platform/serial/ReplaySerialTransport.*`（脚本回放）  

---

## 2.5 传票链路（新增业务路径）

说明：

1. 传票属于 **钥匙协议新增业务路径**，不挂靠 `Q_TASK` 语义。  
2. 工作台 JSON 通过 `TicketIngressService -> TicketStore` 进入主程序。  
3. 手动/自动传票由 `KeyManageController` 编排。  
4. 多帧 ACK 驱动续发统一放在 `KeySerialClient` 内部。  
5. 详细规则见：`TICKET_PROTOCOL_GUIDE.md`。  

---

## 3. 协议是怎么组成的（你问的重点）

## 3.1 帧结构

协议帧格式（来自 `KeyProtocolDefs.h` 与 `KeySerialClient.h`）：

```text
[7E 6C] [KeyId 1B] [FrameNo 1B] [Addr2 2B] [Len 2B LE] [Cmd 1B] [Data N B] [CRC16 2B BE]
```

字段解释：

1. `7E 6C`：固定帧头。  
2. `KeyId`：钥匙柜编号，`0xFF` 代表广播。  
3. `FrameNo`：发送序号，每次发送自增。  
4. `Addr2`：2 字节扩展地址（落帧顺序低字节在前）。  
5. `Len`：小端 2 字节，值 = `Cmd(1) + Data(N)`。  
6. `Cmd`：命令码。  
7. `Data`：命令负载。  
8. `CRC16`：大端 2 字节，校验范围仅 `Cmd + Data`。  

关键提醒：

1. **不同命令的报文并不一样**。  
2. 差异主要体现在 `Addr2`、`Len`、`Data`、`CRC`。  
3. `KeyId` 与 `FrameNo` 在不同会话中也可能不同，不能拿整帧做死比较。  

### 3.2 大小端规则

1. `Len` 是 **LE 小端**。  
2. `CRC16` 是 **BE 大端**。  
3. `Addr2` 写入顺序是 `Lo, Hi`。  

### 3.3 命令码（当前实现）

1. `SET_COM = 0x0F`：握手命令。  
2. `Q_TASK = 0x04`：查询任务列表。  
3. `I_TASK_LOG = 0x05`：请求任务回传日志。  
4. `DEL = 0x06`：删除任务。  
5. `ACK = 0x5A`：设备确认。  
6. `NAK = 0x00`：设备拒绝。  
7. `KEY_EVENT = 0x11`：钥匙在位/离位事件。  
8. `UP_TASK_LOG = 0x15`：回传任务日志。  
9. `INIT = 0x02`：初始化电脑钥匙。  
10. `DN_RFID = 0x1A`：下载 RFID/采码数据文件。  

### 3.4 Addr2 规则

1. `SET_COM / Q_TASK`：`0x0000`。  
2. `DEL / I_TASK_LOG`：当前站号（Lo=stationId, Hi=0x00）。  

### 3.5 NAK 常见错误码

1. `0x03`：CRC 校验失败。  
2. `0x04`：长度不匹配。  
3. `0x08`：GUID 不存在。  

### 3.6 命令级报文组成（最重要）

下面是“同一个协议头下，不同命令的具体差异”。  
说明：以下 CRC 参考来自你当前项目验证样本与现有回归文档。

1. `SET_COM` 请求  
   - `Cmd=0x0F`  
   - `Addr2=0x0000`  
   - `Data=01`（1 字节）  
   - `Len=0x0002`（Cmd 1 + Data 1）  
   - `CRC=1C11`  
   - 示例：`7E 6C FF 00 00 00 02 00 0F 01 1C 11`

2. `Q_TASK` 查询全部请求  
   - `Cmd=0x04`  
   - `Addr2=0x0000`  
   - `Data=FF * 16`  
   - `Len=0x0011`（Cmd 1 + Data 16）  
   - `CRC=F326`  
   - 示例：`7E 6C FF <FrameNo> 00 00 11 00 04 FF..FF F3 26`

3. `I_TASK_LOG` 请求  
   - `Cmd=0x05`
   - `Addr2=stationId`
   - `Data=taskId(16字节)`
   - `Len=0x0011`
   - 当前单帧主链已接入，多帧回放已验证

4. `DEL` 删除请求  
   - `Cmd=0x06`  
   - `Addr2=stationId`（和 Q_TASK 不同）  
   - `Data=taskId(16字节)`  
   - `Len=0x0011`  
   - `CRC` 随 taskId 变化  
   - 参考：taskId 全 0 时 CRC 可为 `1A8C`

5. `ACK(SET_COM)` 响应  
   - `Cmd=0x5A`  
   - `Data=0x0F`（被确认命令）  
   - `Len=0x0002`  
   - `CRC=A720`

6. `ACK(DEL)` 响应  
   - `Cmd=0x5A`  
   - `Data=0x06`  
   - `Len=0x0002`  
   - `CRC=3609`

7. `Q_TASK` 响应（count=0）  
   - `Cmd=0x04`  
   - `Data=00`（任务数）  
   - `Len=0x0002`  
   - `CRC=D14C`

8. `Q_TASK` 响应（count=1）  
   - `Cmd=0x04`  
   - `Data = count(1) + item(20)`  
   - `item = taskId(16) + status(1) + type(1) + source(1) + reserved(1)`  
   - `Len=0x0016`（Cmd 1 + Data 21）

9. `KEY_EVENT` 放上  
   - `Cmd=0x11`  
   - `Data=01 00 00 00`  
   - `Len=0x0005`  
   - `CRC=D162`

10. `KEY_EVENT` 拿走  
   - `Cmd=0x11`  
   - `Data=00 00 00 00`  
   - `Len=0x0005`  
   - `CRC=44A0`

11. `NAK` 响应  
   - `Cmd=0x00`  
   - `Data` 至少包含 `origCmd`，常见还会带 `errCode`  
   - `Len` 与 data 字节数相关（不是固定值）  
   - 解析时要按实际 `Len` 读取，不能写死 2 字节。  

说明：

1. 上述组成规则来自“当前代码 + 你提供的历史文档交叉验证”。  
2. 对于你历史文档里涉及的长报文命令（如 `0x03` 传票、`0x05/0x15` 回传、`0x1A` 下载 RFID、`0x02` 初始化），应优先以当前代码和最新样本为准。  
3. 当前主程序里 `INIT / 下载 RFID` 已接入，但仍是**手工链**，不要擅自视为 ready 自动前置链。  

---

## 4. 一个实际帧怎么读

示例（日志中常见 SET_COM 发送帧）：

```text
7E 6C FF 00 00 00 02 00 0F 01 1C 11
```

拆解：

1. `7E 6C`：帧头。  
2. `FF`：KeyId（广播）。  
3. `00`：FrameNo。  
4. `00 00`：Addr2 = 0x0000。  
5. `02 00`：Len = 0x0002（小端，表示 Cmd+Data 共 2 字节）。  
6. `0F`：Cmd = SET_COM。  
7. `01`：Data（握手数据）。  
8. `1C 11`：CRC16（大端）。  

---

## 5. 状态机和时序（改逻辑前先看）

## 5.1 主流程

1. `connectPort` 打开串口。  
2. 发送 **第一次 `SET_COM`**（初始握手）。  
3. 收到 `ACK(SET_COM)` → `sessionReady=true`。  
4. 收到 `KEY_EVENT(放上)` → `sessionReady=false`，同步启动稳定性窗口；延迟 `KEY_PLACED_HANDSHAKE_DELAY_MS=600ms` 后发 **第二次 `SET_COM`** 握手。  
5. 收到第二次 `ACK(SET_COM)` → `sessionReady=true`（重新就绪）。  
6. 稳定性窗口到期（`KEY_STABLE_WINDOW_MS=600ms` 内无噪声 `KEY_EVENT`） → `keyStable=true` → 允许 `Q_TASK / DEL`。  
7. `KEY_EVENT(拿走)` → 重置所有状态（`keyPlaced/keyStable/sessionReady` 全 false），串口保持连接。  

⚠️ 注意：步骤 4→5 的二次握手是强制的——只凭首次连接的 `sessionReady` 不足以发业务命令，必须等钥匙放上并完成第二次握手后才进入 `keyStable`。  

## 5.2 关键时序常量（来自 `KeySerialClient.h`）

1. `RETRY_TIMEOUT_MS = 1000`。  
2. `MAX_RETRIES = 3`。  
3. `KEY_STABLE_WINDOW_MS = 600`。  
4. `KEY_QUIET_WINDOW_MS = 400`。  
5. `KEY_PLACED_HANDSHAKE_DELAY_MS = 600`。  
6. `STARTUP_PROBE_DELAY_MS = 200`。  
7. `QTASK_RECOVERY_DELAY_MS = 500`。  
8. `QTASK_RECOVERY_ROUNDS = 2`。  

## 5.3 诊断与恢复机制（已实现）

1. `drain`：业务发送前清空接收缓冲，减少残留干扰。  
2. `resync`：拆包遇坏帧时滑窗重同步。  
3. `startup probe`：SET_COM ACK 后若无 KEY_EVENT，主动探测 Q_TASK。  
4. `Q_TASK soft timeout`：钥匙在位时延时重试，最多 2 轮。  

---

## 6. 日志体系（串口报文页怎么来的）

## 6.1 日志结构

`LogItem` 字段：

1. `timestamp`  
2. `dir`（TX/RX/RAW/EVT/WARN/ERR）  
3. `cmd`  
4. `length`  
5. `summary`  
6. `hex`  
7. `crcOk`  
8. `expertOnly`  
9. `opId`（用于关联一次请求动作）  

## 6.2 页面表头映射

串口报文表头固定为：

1. 时间  
2. 方向  
3. OpId  
4. 命令  
5. 长度  
6. 摘要  
7. Hex  

## 6.3 专家模式与 Hex 显示

1. 专家模式：`SerialLogManager::shouldDisplay` 过滤 `expertOnly` 行。  
2. 显示 Hex：`keymanagepage::onShowHexToggled` 控制第 7 列隐藏/显示与多行展示。  
3. Hex 双击复制：双击 Hex 列会复制原始 hex（`Qt::UserRole`）。  

---

## 7. 新增一个协议命令时该怎么改（最小改动路径）

按顺序改，避免跨层乱改：

### 步骤 1：定义协议常量

文件：`KeyProtocolDefs.h`  
动作：新增命令码、必要错误码或地址扩展常量。  

### 步骤 2：协议层实现

文件：`KeySerialClient.h/.cpp`  
动作：

1. 增加发送入口函数（如果需要）。  
2. 在 `handleFrame` 中增加响应解析分支。  
3. 更新超时/重试与 inFlight 匹配逻辑。  
4. 通过 `emitLog` 输出结构化日志。  

### 步骤 3：服务层透传

文件：`IKeySessionService.h`、`KeySessionService.cpp`  
动作：

1. 在 `CommandId`/`execute` 映射新增命令入口。  
2. 保持 UI 不直接碰协议类。  

### 步骤 4：控制器触发

文件：`KeyManageController.cpp`  
动作：新增按钮对应动作并组装 `CommandRequest`。  

### 步骤 5：页面展示

文件：`keymanagepage.cpp`  
动作：只做按钮连接、状态提示和日志展示，不写协议细节。  

---

## 8. 无硬件验证（回放）

回放相关文档见：`docs/replay/REPLAY_SPEC.md`。  
这里给最小启动信息：

1. `serial/replayEnabled=true`。  
2. `serial/replayScript=test/replay/<scenario>.jsonl`  
   默认值（代码写死）：`test/replay/scenario_half_and_sticky.jsonl`。  
   多帧回传验证可切到：`test/replay/scenario_up_task_log_multiframe.jsonl`  
3. 若脚本加载失败，`KeySessionService` 自动回退真实串口。  

---

## 9. 改动后验收清单（必须）

1. 编译通过。  
2. `.\tools\arch_guard.ps1 -Phase 3` 输出 `No violations.`。  
3. 基础链路验证：  
   - 初始化握手（SET_COM）  
   - 查询任务（Q_TASK）  
   - 删除任务（DEL）  
4. 串口报文表格验证：  
   - 日志能追加  
   - 专家模式过滤正常  
   - Hex 显示/隐藏正常  
   - CSV 导出正常  
5. 若本次改动引用了历史文档结论，必须补一条“当前代码实测结果”到变更说明（避免历史结论误用）。  

---

## 10. 常见误区（直接避免）

1. 在 `keymanagepage` 直接 include/调用 `KeySerialClient`。  
2. 在 `protocol` 层 include `QtSerialTransport` 实现类。  
3. 修改 `Len` 或 `CRC` 规则但不做回归。  
4. 把业务逻辑写进 UI 层（比如按钮里直接发帧）。  
5. 改了协议但不补结构化日志，导致无法诊断。  
6. 误以为所有命令 payload 一样，导致 `Len/CRC` 算错。  
7. 用"完整帧全字节相等"做唯一判据，忽略 `FrameNo` 可变性。  
8. 以为 `CommandId::SetCom` 会真正重发 SET_COM 帧——**当前 `KeySessionService::execute()` 的 `SetCom` case 只做 `disconnectPort()`（已知 gap，见代码 NOTE 注释），不会主动发帧**。如需主动重握手，应直接调用 `connectPort` 或为 `KeySerialClient` 新增公开接口，而不是依赖 `execute(SetCom)`。  
---

## 11. 关联文档

1. 目录落地：`docs/architecture/CURRENT_FOLDER_LAYOUT.md`。  
2. 文件索引：`docs/architecture/CODEMAP.md`。  
3. 依赖红线：`docs/architecture/DEPENDENCY_RULES.md`。  
4. 注释规范：`docs/COMMENTING_GUIDE.md`。  
5. 回放规范：`docs/replay/REPLAY_SPEC.md`。  
