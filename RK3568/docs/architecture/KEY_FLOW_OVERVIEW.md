# 钥匙与传票主链路总览（KEY FLOW OVERVIEW）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-06  
适用范围：说明当前钥匙链路、工作台 JSON 输入链、系统票链、传票发送链。  
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

## 4. 自动传票链（预埋）

```text
TicketIngressService 收到新 JSON
  -> TicketStore 入池
    -> KeyManageController::handleJsonReceived()
      -> tryAutoTransferPendingTicket()
        -> tryStartTicketTransfer(...)
```

当前状态：

1. 逻辑已预埋  
2. 默认关闭：`ticket/autoTransferEnabled=false`  

## 5. 传票发送状态

当前系统票 `transferState`：

1. `received`
2. `auto-pending`
3. `sending`
4. `success`
5. `failed`
