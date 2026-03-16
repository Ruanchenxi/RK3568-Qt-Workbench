# 钥匙与传票主链路总览（KEY FLOW OVERVIEW）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-13  
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
3. 当前自动回传与手工回传都已收紧为“整把钥匙任务门禁”：
   - 只要钥匙中的相关任务里仍有任意一条是 `0x00/0x01`
   - 当前就不允许开始任何回传
   - 会统一提示“任务未全部完成，暂不回传，请全部任务完成后再放回钥匙”
4. 若本地系统票仍停在 `sending/auto-pending/failed`，但 `Q_TASK` 已确认钥匙中存在该任务，则先自动对账修正为 `success`。  
5. 若 `Q_TASK` 明确返回钥匙中无任务，而本地系统票仍卡在 `sending/failed`，则自动回退为 `auto-pending` 并再试一次自动传票。  
6. 当前自动回传在读取日志前会先补一次 `SET_COM`，以贴近原产品在差底座场景下的恢复方式。  
7. 多帧 `UP_TASK_LOG` 当前代码已支持按 `seq` 累积，并已通过 replay 场景验证。  

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
