# 钥匙与传票主链路总览（KEY FLOW OVERVIEW）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-04-03  
适用范围：说明当前钥匙链路、工作台 JSON 输入链、系统票链、传票发送链、回传链。  
不适用范围：不替代逐字节协议文档。  

## 1. 旧钥匙串口链

```text
KeyManagePage
  -> KeyManageController
    -> IKeySessionService
      -> KeySessionService
        -> KeySerialClient
          -> ISerialTransport
            -> QtSerialTransport / ReplaySerialTransport
```

用途：

1. `SET_COM`
2. `Q_TASK`
3. `DEL`
4. 串口日志

## 2. 工作台 JSON 输入链

```text
Workbench(QWebEngine 黑盒页面)
  -> 本地 HTTP POST
    -> TicketIngressService
      -> TicketStore
        -> KeyManageController
          -> KeyManagePage
```

用途：

1. 接住工作台发出的传票 JSON  
2. 落盘 `logs/http_ticket_*.json`  
3. 展示到 `HTTP服务端报文`  
4. 入 `系统票数据`  
5. 票池执行态持久化到 `logs/ticket_store.json`  

当前入口约束：

1. 当前正式入口只接：
   - `POST ticket/httpIngressPath`
   - `POST ticket/httpCancelPath`
2. 错路径会返回：
   - `404`
3. 错方法会返回：
   - `405`
4. 坏请求（例如请求行非法、`Content-Length` 非法、JSON 无法入池）会返回：
   - `400`
5. 当前只有在系统票真实入池成功后，HTTP 才返回：
   - `success=true`
6. 启动日志会打印当前实际监听的：
   - `host`
   - `port`
   - `path`

### 2.1 工作台撤销输入链

```text
Workbench(QWebEngine 黑盒页面)
  -> 本地 HTTP POST /ywticket/WebApi/cancelTicket
    -> TicketIngressService
      -> KeyManageController cancel handler
        -> TicketCancelCoordinator
          -> TicketStore / Q_TASK / DEL / Q_TASK verify
```

用途：

1. 接住工作台发出的撤销请求  
2. 校验 `taskId` 是否存在、是否已完成、是否已在清理链中  
3. 对“未下发且确认不在钥匙中”的票直接移除  
4. 对“已进钥匙或疑似已进钥匙”的票进入：
   - `cancel-accepted`
   - `cancel-pending`
   - `cancel-executing`
   - `cancel-done`
5. 最小撤销记录当前保存在：
   - `logs/cancel_ticket_journal.json`
6. 若本地票不存在，但钥匙中仍能对账到该任务，会先做孤儿恢复：
   - `source = orphan-recovery`

当前规则：

1. ingress 只做：
   - 校验
   - 去重
   - 受理
   - 登记/入队
2. 不在 HTTP ingress 中同步等待串口删除结果  
3. 已在回传清理链中的票不再新起撤销删除链  
4. 已完成回传闭环的票直接幂等返回“任务已完成，无需撤销”  
5. `cancel-accepted / cancel-pending / cancel-executing` 会冻结自动传票、自动回传与自动恢复  

补充说明（2026-04-03）：

1. 当前本地 HTTP 接收已按更严格语义收口：
   - 错路径：`404`
   - 错方法：`405`
   - 坏请求 / 非法 JSON / `taskId` 为空：`400`
2. `TicketIngressService` 当前会把“HTTP 收到请求”和“业务成功入池”区分开：
   - 只有 `TicketStore` 真正接受后，才返回 `success=true`
   - 若业务拒绝，则返回 `success=false`
3. 启动日志当前打印真实配置的：
   - `ticket/httpIngressHost`
   - `ticket/httpIngressPort`
   - `ticket/httpIngressPath`

## 3. 系统票 -> 手动传票链

```text
KeyManagePage(systemTicketTable 选中)
  -> KeyManageController::onTransferSelectedTicket()
    -> IKeySessionService::transferTicket(...)
      -> KeySessionService
        -> KeySerialClient::transferTicketJson(...)
          -> TicketPayloadEncoder
          -> TicketFrameBuilder
          -> 串口发送 / ACK 续发
```

要点：

1. 页面层只传“选中哪条系统票”  
2. 原始 JSON 由 `TicketStore` 提供  
3. 协议状态机只在 `KeySerialClient` 内维护  
4. `DEL` 当前按“钥匙票表选中项”执行，不再默认删除第一条钥匙任务  
5. 传票链当前会先补一次 `SET_COM`，确认 ACK 后再发 `TICKET`，以提高空闲后一发首包成功率  

## 4. 自动传票链

```text
TicketIngressService 收到新 JSON
  -> TicketStore 入池
    -> KeyManageController::handleJsonReceived()
      -> tryAutoTransferPendingTicket()
        -> tryStartTicketTransfer(...)
```

当前状态：

1. 逻辑已落地  
2. 默认开启：`ticket/autoTransferEnabled=true`  
3. 进入发送前必须同时满足：
   - 串口已连接
   - 钥匙在位
   - 钥匙稳定
   - `sessionReady=true`
4. 当前自动传票会尊重 `cancelState`：
   - `cancel-accepted`
   - `cancel-pending`
   - `cancel-executing`
   这些状态下不会继续自动补传  
5. 传票执行态当前由 `TicketStore` 持久化；程序启动时会在 `KeyManageController::start()` 中加载  

## 5. 自动回传链（当前主程序）

```text
钥匙放回
  -> KEY_EVENT(placed)
    -> SET_COM ready
      -> KeyManageController 在 ready 边沿自动触发一次 Q_TASK
        -> 若发现钥匙里已存在对应任务，则先对账修正本地 transferState
        -> 若确认钥匙里没有任务，但本地系统票仍卡在 sending/failed，则回退为 auto-pending 并自动补传
        -> Q_TASK(status=0x02)
          -> IKeySessionService::execute(SetCom)
            -> SET_COM ACK
              -> IKeySessionService::execute(QueryTaskLog)
                -> KeySerialClient::requestTaskLog(...)
                  -> I_TASK_LOG(0x05)
                  -> ACK(5A 05 00 00)
                  -> UP_TASK_LOG(0x15)
                    -> KeyManageController::handleSessionEvent(TaskLogReady)
                      -> TicketReturnHttpClient
                        -> HTTP /finish-step-batch
                          -> DEL
```

要点：

1. 自动回传当前以 `Q_TASK.status=0x02` 作为门禁。  
2. 钥匙放回并完成握手后，不再要求用户手工点“读取钥匙列表”才开始回传。  
3. 当前回传门禁已调整为“按任务单号独立回传”：
   - 手工回传只看当前选中任务是否已完成，不再要求整把钥匙全部完成
   - 自动回传会从 `Q_TASK.status=0x02` 的任务中选择一条作为目标任务
   - 其他未完成任务不会阻塞当前已完成任务回传
4. 当前自动回传仍保持“单链串行”：
   - 同一时刻只允许一条回传链在途
   - 若已有任务处于 `return-handshake / return-log-requesting / return-log-receiving / return-uploading / return-delete-verifying`
   - 下一条已完成任务要等上一条回传链完全收口后才会继续
5. 若本地系统票仍停在 `sending/auto-pending/failed`，但 `Q_TASK` 已确认钥匙中存在该任务，则先自动对账修正为 `success`。  
6. 若 `Q_TASK` 明确返回钥匙中无任务，而本地系统票仍卡在 `sending/failed`，则自动回退为 `auto-pending` 并再试一次自动传票。  
7. 当前自动回传在读取日志前会先补一次 `SET_COM`，以贴近原产品在差底座场景下的恢复方式。  
8. 多帧 `UP_TASK_LOG` 当前代码已支持按 `seq` 累积，并已通过 replay 场景验证。  
9. 回传上传失败当前会进入 `manual-required`，不自动越过人工确认边界。  

## 6. 手工初始化 / 下载 RFID 链

```text
KeyManagePage(点击“初始化”/“下载 RFID”)
  -> KeyManageController
    -> KeyProvisioningService
      -> HTTP /package-data 或 /rfid-data
        -> InitPayloadEncoder / RfidPayloadEncoder
          -> IKeySessionService::execute(...)
            -> KeySessionService
              -> KeySerialClient
                -> INIT(0x02) / DN_RFID(0x1A)
```

要点：

1. 这两条链当前为**手工链**，不是 ready 后默认自动前置链。  
2. `INIT(0x02)` 当前按原产品样本走多帧发送与 ACK 闭环。  
3. `DN_RFID(0x1A)` 当前已完成单帧真机验证。  
4. 接入后自动回传零回归已通过。  

## 7. 传票发送状态

当前系统票 `transferState`：

1. `received`
2. `auto-pending`
3. `sending`
4. `success`
5. `failed`
6. `key-cleared`

补充说明：

1. `transferState` 是主程序对当前传票执行态的本地判断，不是钥匙侧绝对真值。  
2. 当前以 `Q_TASK` 结果作为对账依据修正本地状态。  
3. 执行态当前会落盘到 `logs/ticket_store.json`，重启后会在启动阶段加载。  


Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-16  
适用范围：说明当前钥匙链路、工作台 JSON 输入链、系统票链、传票发送链、回传链。  
不适用范围：不替代逐字节协议文档。  

## 1. 旧钥匙串口链

```text
KeyManagePage
  -> KeyManageController
    -> IKeySessionService
      -> KeySessionService
        -> KeySerialClient
          -> ISerialTransport
            -> QtSerialTransport / ReplaySerialTransport
```

用途：

1. `SET_COM`
2. `Q_TASK`
3. `DEL`
4. 串口日志

## 2. 工作台 JSON 输入链

```text
Workbench(QWebEngine 黑盒页面)
  -> 本地 HTTP POST
    -> TicketIngressService
      -> TicketStore
        -> KeyManageController
          -> KeyManagePage
```

用途：

1. 接住工作台发出的传票 JSON  
2. 落盘 `logs/http_ticket_*.json`  
3. 展示到 `HTTP服务端报文`  
4. 入 `系统票数据`  

当前入口约束：

1. 当前正式入口只接：
   - `POST ticket/httpIngressPath`
   - `POST ticket/httpCancelPath`
2. 错路径会返回：
   - `404`
3. 错方法会返回：
   - `405`
4. 坏请求（例如请求行非法、`Content-Length` 非法、JSON 无法入池）会返回：
   - `400`
5. 当前只有在系统票真实入池成功后，HTTP 才返回：
   - `success=true`
6. 启动日志会打印当前实际监听的：
   - `host`
   - `port`
   - `path`

### 2.1 工作台撤销输入链

```text
Workbench(QWebEngine 黑盒页面)
  -> 本地 HTTP POST /ywticket/WebApi/cancelTicket
    -> TicketIngressService
      -> KeyManageController cancel handler
        -> TicketCancelCoordinator
          -> TicketStore / Q_TASK / DEL / Q_TASK verify
```

用途：

1. 接住工作台发出的撤销请求  
2. 校验 `taskId` 是否存在、是否已完成、是否已在清理链中  
3. 对“未下发且确认不在钥匙中”的票直接移除  
4. 对“已进钥匙或疑似已进钥匙”的票进入：
   - `cancel-accepted`
   - `cancel-pending`
   - `cancel-executing`
   - `cancel-done`
5. 最小撤销记录当前保存在：
   - `logs/cancel_ticket_journal.json`

当前规则：

1. ingress 只做：
   - 校验
   - 去重
   - 受理
   - 登记/入队
2. 不在 HTTP ingress 中同步等待串口删除结果  
3. 已在回传清理链中的票不再新起撤销删除链  
4. 已完成回传闭环的票直接幂等返回“任务已完成，无需撤销”  

补充说明（2026-03-23）：

1. 当前本地 HTTP 接收已按更严格语义收口：
   - 错路径：`404`
   - 错方法：`405`
   - 坏请求 / 非法 JSON / `taskId` 为空：`400`
2. `TicketIngressService` 当前会把“HTTP 收到请求”和“业务成功入池”区分开：
   - 只有 `TicketStore` 真正接受后，才返回 `success=true`
   - 若业务拒绝，则返回 `success=false`
3. 启动日志当前打印真实配置的：
   - `ticket/httpIngressHost`
   - `ticket/httpIngressPort`
   - `ticket/httpIngressPath`

## 3. 系统票 -> 手动传票链

```text
KeyManagePage(systemTicketTable 选中)
  -> KeyManageController::onTransferSelectedTicket()
    -> IKeySessionService::transferTicket(...)
      -> KeySessionService
        -> KeySerialClient::transferTicketJson(...)
          -> TicketPayloadEncoder
          -> TicketFrameBuilder
          -> 串口发送 / ACK 续发
```

要点：

1. 页面层只传“选中哪条系统票”  
2. 原始 JSON 由 `TicketStore` 提供  
3. 协议状态机只在 `KeySerialClient` 内维护  
4. `DEL` 当前按“钥匙票表选中项”执行，不再默认删除第一条钥匙任务  

## 4. 自动传票链

```text
TicketIngressService 收到新 JSON
  -> TicketStore 入池
    -> KeyManageController::handleJsonReceived()
      -> tryAutoTransferPendingTicket()
        -> tryStartTicketTransfer(...)
```

当前状态：

1. 逻辑已落地  
2. 默认开启：`ticket/autoTransferEnabled=true`
3. 进入发送前必须同时满足：
   - 串口已连接
   - 钥匙在位
   - 钥匙稳定
   - `sessionReady=true`

## 5. 自动回传链（当前主程序）

```text
钥匙放回
  -> KEY_EVENT(placed)
    -> SET_COM ready
      -> KeyManageController 在 ready 边沿自动触发一次 Q_TASK
        -> 若发现钥匙里已存在对应任务，则先对账修正本地 transferState
        -> 若确认钥匙里没有任务，但本地系统票仍卡在 sending/failed，则回退为 auto-pending 并自动补传
        -> Q_TASK(status=0x02)
          -> IKeySessionService::execute(SetCom)
            -> SET_COM ACK
              -> IKeySessionService::execute(QueryTaskLog)
                -> KeySerialClient::requestTaskLog(...)
                  -> I_TASK_LOG(0x05)
                  -> ACK(5A 05 00 00)
                  -> UP_TASK_LOG(0x15)
                    -> KeyManageController::handleSessionEvent(TaskLogReady)
                      -> TicketReturnHttpClient
                        -> HTTP /finish-step-batch
                          -> DEL
```

要点：

1. 自动回传当前以 `Q_TASK.status=0x02` 作为门禁。  
2. 钥匙放回并完成握手后，不再要求用户手工点“读取钥匙列表”才开始回传。  
3. 当前回传门禁已调整为“按任务单号独立回传”：
   - 手工回传只看当前选中任务是否已完成，不再要求整把钥匙全部完成
   - 自动回传会从 `Q_TASK.status=0x02` 的任务中选择一条作为目标任务
   - 其他未完成任务不会阻塞当前已完成任务回传
4. 当前自动回传仍保持“单链串行”：
   - 同一时刻只允许一条回传链在途
   - 若已有任务处于 `return-handshake / return-log-requesting / return-log-receiving / return-uploading / return-delete-verifying`
   - 下一条已完成任务要等上一条回传链完全收口后才会继续
5. 若本地系统票仍停在 `sending/auto-pending/failed`，但 `Q_TASK` 已确认钥匙中存在该任务，则先自动对账修正为 `success`。
6. 若 `Q_TASK` 明确返回钥匙中无任务，而本地系统票仍卡在 `sending/failed`，则自动回退为 `auto-pending` 并再试一次自动传票。
7. 当前自动回传在读取日志前会先补一次 `SET_COM`，以贴近原产品在差底座场景下的恢复方式。
8. 多帧 `UP_TASK_LOG` 当前代码已支持按 `seq` 累积，并已通过 replay 场景验证。

## 6. 手工初始化 / 下载 RFID 链

```text
KeyManagePage(点击“初始化”/“下载 RFID”)
  -> KeyManageController
    -> KeyProvisioningService
      -> HTTP /package-data 或 /rfid-data
        -> InitPayloadEncoder / RfidPayloadEncoder
          -> IKeySessionService::execute(...)
            -> KeySessionService
              -> KeySerialClient
                -> INIT(0x02) / DN_RFID(0x1A)
```

要点：

1. 这两条链当前为**手工链**，不是 ready 后默认自动前置链。  
2. `INIT(0x02)` 当前按原产品样本走多帧发送与 ACK 闭环。  
3. `DN_RFID(0x1A)` 当前已完成单帧真机验证。  
4. 接入后自动回传零回归已通过。  

## 7. 传票发送状态

当前系统票 `transferState`：

1. `received`
2. `auto-pending`
3. `sending`
4. `success`
5. `failed`
6. `key-cleared`

说明：

1. `key-cleared` 表示钥匙侧对应任务已被手动删除  
2. 此状态下系统票允许再次传票  
3. 当前下一阶段重点是回传链路，不在这一阶段继续扩展撤销规则  

当前系统票 `cancelState`：

1. `none`
2. `cancel-accepted`
3. `cancel-pending`
4. `cancel-executing`
5. `cancel-done`
6. `cancel-failed`

说明：

1. `cancel-accepted/cancel-pending/cancel-executing` 会冻结：
   - 自动传票
   - 自动回传
   - 自动恢复补传
2. 当前只在单钥匙链路上执行删除，但已预埋：
   - `residentSlotId`
   - `residentKeyId`

## 8. 回传状态

当前系统票 `returnState`：

1. `idle`
2. `return-requesting-log`
3. `return-uploading`
4. `return-upload-success`
5. `return-delete-pending`
6. `return-delete-success`
7. `return-success`（兼容旧状态，逐步退出）
8. `return-failed`

## 9. 登录与本地输入辅助链

### 9.1 登录账号选择链

```text
LoginPage(点击“选择”)
  -> LoginController::requestAccountList()
    -> AuthService::fetchAccountList(...)
      -> HTTP /list-account
        -> QMenu 下拉菜单
          -> 回填 usernameEdit
```

要点：

1. 当前账号列表接口属于登录辅助链，不改变原有手输登录主链。  
2. 账号仍可手工输入；`选择账号` 只是辅助输入入口。  
3. `list-account` 默认回退到：
   - `system/apiUrl + /list-account`
4. 若现场接口单独部署，可通过配置项：
   - `auth/accountListUrl`
   指向完整地址。  

### 9.2 虚拟键盘边界（当前主线）

```text
本地 Qt Widgets 输入页
  -> 自定义 QWidget 键盘
    -> 顶部统一入口
    -> 字段分型
    -> 页面局部避让
    -> 当前输入框保持可见

工作台 QWebEngine 页
  -> 特例 bridge
  -> 仅在网页当前焦点位于可编辑元素时允许打开

钥匙管理 / 服务日志页
  -> 当前不作为有效输入页
  -> 不提供“弹出后可直接输入”的正式承诺
```

要点：

1. 当前正式主线是：
   - 应用内自定义 QWidget 键盘
   - 主窗口底部停靠式非模态面板
   - 页面自动让位，但不做整页缩放
   - 跨页切换时统一收起键盘并清空上一个页面的输入上下文
2. 页面能力当前按分层处理：
   - 登录页：完整支持
   - 系统设置页：完整支持
   - 工作台页：仅在网页当前焦点位于可编辑元素时允许通过 bridge 输入
   - 钥匙管理页：当前不作为有效输入页
   - 服务日志页：当前不支持
3. 键盘采用字段分型，不只做一种传统全键盘：
   - 账号：优先选择
   - 数字字段：数字键盘
   - URL/IP/接口地址：URL 专用键盘
   - 密码/普通文本：全键盘
4. 当前代码落地的第一阶段已完成基础闭环：
   - 本地 Widgets 页主线使用自定义 QWidget 键盘
   - 当前已支持：
     - 登录页
     - 系统设置页
     - 工作台页（仅特例 bridge，需先聚焦网页输入元素）
   - 当前主要支持控件：
     - `QLineEdit`
   - 当前已补基础目标识别：
     - `QComboBox::lineEdit()`
   - 当前已具备：
     - 普通 / 数字 / URL 键盘
     - 模式闭环切换
     - 页面级尺寸策略
     - 固定候选栏中文输入形态
   - 键盘模块按 `domain / application / ui` 分层，详见：
     - `architecture/KEYBOARD_MODULE_ARCHITECTURE.md`
## 钥匙串口配置口径（档 1）

- 当前执行方案已收口为：`保存配置，重启程序后生效`。
- 系统设置页修改 `serial/keyPort` 时，只更新配置态，不在当前运行中热切换钥匙串口。
- 当前运行中的钥匙会话继续保持原 `runtimePort`，直到程序下次启动时才按新配置建立串口连接。
- 若当前存在传票、回传、删除验证、`return-interrupted-retryable`、INIT、下载 RFID 或自动恢复中的链路，允许保存配置，但不建议立即重启。
- 读卡串口 `serial/cardPort` 当前也按同样口径处理：保存配置，重启后生效；不与钥匙串口自动联动。
- 当前不再把“运行中 apply 串口切换”作为面向现场工程师的主交互方案；如后续重新评估热切换，需另立专项方案与回归验证。
