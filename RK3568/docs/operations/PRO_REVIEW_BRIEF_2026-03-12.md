# PRO 审查接手简报（2026-03-12）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-12  
适用范围：提供给 `pro` 做代码审查、业务逻辑 bug 排查、板端联调收口。  
不适用范围：不替代完整协议文档，不替代逐字节报文参考。  

---

## 1. 本次审查的目标

当前不是从零设计，而是已经进入：

1. RK3588 Ubuntu 板端本机编译可用  
2. 工作台 / 传票 / 自动回传主链已基本跑通  
3. 剩余问题集中在：
   - 板端差底座条件下的串口时序稳定性
   - 自动回传与钥匙任务清理的状态机收口
   - “自动失败后，人工兜底”是否完全符合业务预期

本简报的目的，是让 `pro` 直接从“已知事实 + 已复现问题 + 重点怀疑点”入手，而不是重新摸索整个项目背景。

---

## 2. 当前板端环境（已确认）

1. 板端系统：RK3588 Ubuntu 22.04.5 LTS  
2. 架构：`aarch64`  
3. 图形环境：`lightdm + Xorg(:0)` 正常  
4. 串口节点：
   - `/dev/ttyS4`：钥匙链路
   - `/dev/ttyS3`：刷卡链路
5. 用户 `linaro` 已在 `dialout` 组中  
6. 板端已安装：
   - `qt5-qmake`
   - `qtbase5-dev`
   - `libqt5serialport5-dev`
   - `qtwebengine5-dev`
7. 当前已验证可以在板端本机执行：
   - `qmake ../RK3568/RK3568.pro`
   - `make -j$(nproc)`
   - `DISPLAY=:0 ./RK3568`

---

## 3. 当前已确认可工作的链路

### 3.1 启动与 UI

1. 主程序可在 RK3588 Ubuntu 板端成功启动  
2. 登录页可正常登录  
3. `QWebEngineView` 可加载工作台  
4. 工作台页面跳转正常  
5. Linux 板端窗口大小问题已通过运行时覆写最小尺寸做了初步修正

### 3.2 传票链

1. 工作台申请任务后，本地 HTTP 接收成功  
2. JSON 能入系统票池并显示到钥匙管理页  
3. 传票到钥匙当前已能成功发送  
4. 串口日志中可看到：
   - `TICKET frame 1/1 send`
   - `ACK for 0x03`
   - `传票发送完成`

### 3.3 回传链

当前板端已经验证到：

```text
Q_TASK(status=0x02)
  -> I_TASK_LOG(0x05)
  -> ACK(5A 05 00 00)
  -> UP_TASK_LOG(0x15)
  -> HTTP /finish-step-batch
  -> HTTP 200 success
```

实测 HTTP 客户端日志已出现：

1. `正在回传 1 号座钥匙任务数据。。。`
2. JSON body 含：
   - `stationNo`
   - `steps`
   - `taskId`
   - `taskLogItems[].serialNumber/opStatus/opType/rfid/opTime`
3. 服务端返回：
   - `code = 200`
   - `success = true`

说明：  
“回传上传”这一步当前在板端已经不是猜测，而是真正成功过。

---

## 4. 当前已复现的问题（需要 PRO 重点审）

### 4.1 自动传票曾在板端首帧发送处崩溃

已复现过一次 SIGSEGV，gdb 栈显示：

```text
TicketIngressService::jsonReceived
  -> KeyManageController::handleJsonReceived
    -> tryAutoTransferPendingTicket
      -> tryStartTicketTransfer
        -> KeySessionService::transferTicket
          -> KeySerialClient::transferTicketJson
            -> sendNextTicketFrame
              -> sendFrame
```

当前已在本地做过一轮防御性修复：

1. `sendNextTicketFrame()` 不再持有 `m_ticketFrames` 的容器引用跨信号使用  
2. `sendFrame()` 增加了空帧 / transport 空指针防御

`pro` 需要确认：

1. 此修复是否只是掩盖症状，还是已经命中根因  
2. 是否还存在其他同步信号导致 `m_ticketFrames` / in-flight 状态被打乱的问题  

重点文件：

1. `src/features/key/protocol/KeySerialClient.cpp`
2. `src/features/key/application/KeySessionService.cpp`
3. `src/features/key/application/KeyManageController.cpp`

### 4.2 自动回传时，`UP_TASK_LOG` 已到但上下文被清空

板端串口日志已明确出现：

1. `I_TASK_LOG resp: totalFrames=1`
2. `ACK for I_TASK_LOG, start upload from frame 0`
3. 随后钥匙发来：
   - `UP_TASK_LOG recv: payload=46B`
4. 但主程序记录：
   - `UP_TASK_LOG ignored: no active I_TASK_LOG context`

这说明：

1. 钥匙并不是没回日志  
2. 是主程序自己的 task-log 上下文在中途被清掉了

当前已做的本地修复方向：

1. 不再让无关 `Q_TASK / DEL / TICKET` 的 timeout/NAK 清掉 `clearTaskLogState()`  
2. 只在真正 `I_TASK_LOG / UP_TASK_LOG` 失败时清 task-log 上下文

`pro` 需要重点确认：

1. 现有 timeout / NAK 处理是否仍会误伤别的 in-flight 业务上下文  
2. 是否还存在“旧回包 / 旧 timeout / 差底座重复包”污染当前回传链的问题  

重点文件：

1. `src/features/key/protocol/KeySerialClient.cpp`

### 4.3 回传 HTTP 成功，但钥匙任务不一定被清掉

当前板端已出现这种状态：

1. 系统票显示：
   - `已回传到系统，并准备清理钥匙任务`
2. 但下方 `钥匙票数据` 中，对应任务仍然存在，状态仍是 `status=0x02`

这说明：

1. `HTTP /finish-step-batch` 成功不等于闭环完成  
2. 最后一步 `DEL` 清理钥匙任务并不稳定，或后续状态没有收干净

`pro` 需要重点确认：

1. `handleReturnUploadSucceeded()` 后是否一定会发 `DEL`  
2. 发 `DEL` 之后是否一定会绑定“待验证删除”的本地上下文  
3. 自动回传成功后的系统票文案和状态定义是否过早进入“准完成”态  

重点文件：

1. `src/features/key/application/KeyManageController.cpp`
2. `src/features/key/protocol/KeySerialClient.cpp`
3. `src/features/key/application/TicketStore.cpp`

### 4.4 手动回传按钮原本一直是灰的（UI 逻辑 bug）

实际业务要求是：

1. 钥匙放回后，自动尝试回传  
2. 自动失败后，用户可以：
   - 手动读取钥匙票列表
   - 手动点击 `回传(重试)` 做兜底

但板端实际观察到：

1. 某些 `return-failed` 场景下，`回传(重试)` 按钮仍是灰的  

原因已确认：

1. controller 逻辑里允许手动回传  
2. 但 UI 层 `btnReturnTicket` 在 `.ui` 中默认 disabled  
3. 页面代码原本没有根据 `return-failed / success / return-success` 去重新 enable

当前已在本地补了 UI 使能逻辑。

`pro` 需要确认：

1. 手动兜底业务规则是否应该按：
   - `transferState == success`
   - 且 `returnState != return-success / return-requesting-log / return-uploading`
   来放开按钮
2. 是否还需要更严格地要求“必须先手动读取钥匙票列表后才允许点回传”

重点文件：

1. `src/features/key/ui/keymanagepage.cpp`
2. `ui/keymanagepage.ui`

### 4.5 Linux 板端下 `waitForBytesWritten()` 与真实有效响应并存

板端串口日志里反复出现这种组合：

1. `ACK for SET_COM`
2. `ACK for DEL`
3. `Q_TASK resp: count=...`
4. 甚至 `UP_TASK_LOG recv`

但同时又出现：

1. `SerialError(12): Operation timed out`
2. `write: xx/xx bytes, wait=FAIL`
3. `retry write: xx/0 bytes, wait=FAIL`

这说明：

1. 在当前 RK3588 Ubuntu + `/dev/ttyS4` + 差底座环境下  
2. “写等待超时”并不总等于“协议发送失败”

`pro` 需要重点看：

1. 当前状态机是否把 `waitForBytesWritten()` 看得过重  
2. 当“有效响应已收到”与“写等待失败”同时出现时，应以哪个为准  
3. `SerialError(12)` 是否应该从“业务失败依据”降级为“诊断告警”

重点文件：

1. `src/platform/serial/QtSerialTransport.cpp`
2. `src/features/key/protocol/KeySerialClient.cpp`

---

## 5. 当前板端业务表现（给 PRO 的一句话）

当前更像是：

> 主链已经可通，但在差底座 + Linux 串口时序脏条件下，状态机仍会被旧 timeout / 旧回包 / 写等待异常污染，导致自动回传与钥匙任务清理不稳定。

不是：

> 功能完全没实现

这点非常重要，会决定审查方向是“继续补功能”，还是“梳理成功/失败判据与状态收口”。

---

## 6. 建议 PRO 优先看的文件顺序

1. `src/features/key/protocol/KeySerialClient.cpp`
2. `src/features/key/application/KeyManageController.cpp`
3. `src/features/key/application/TicketStore.cpp`
4. `src/platform/serial/QtSerialTransport.cpp`
5. `src/features/key/ui/keymanagepage.cpp`
6. `ui/keymanagepage.ui`

---

## 7. 建议 PRO 回答的审查问题

1. 自动传票 / 自动回传链中，是否存在“收到旧回包后覆盖当前状态”的风险  
2. timeout / NAK 的清理逻辑，是否错误地清掉了别的业务上下文  
3. `waitForBytesWritten()` 在 Linux 差底座环境下是否被用作了错误的失败判据  
4. 回传成功后，系统票状态是否过早进入“准完成态”  
5. 自动回传失败后，手动兜底链是否完全符合业务预期  
6. `DEL` 清理钥匙任务是否应该视为回传闭环完成的必要条件  
7. UI 当前是否把一些“逻辑允许”的动作错误地锁死了

---

## 8. 当前最值得保留的结论

1. 板端本机编译路线可行，不需要先走交叉编译  
2. `QWebEngineView` 在板端可用，不是当前主问题  
3. 工作台申请任务 -> 主程序本地 HTTP 接收已通  
4. 传票到钥匙已能成功  
5. 自动回传至少有过真实 HTTP 200 success  
6. 当前主要问题集中在“状态机收口与失败兜底”，不是“主链根本跑不通”

