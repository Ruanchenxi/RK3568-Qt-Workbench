# 回传链路协议基线（RETURN PROTOCOL BASELINE）

Status: Active  
Owner: 钥匙链路维护者  
Last Updated: 2026-03-11  
适用范围：主程序“钥匙完成任务后回传日志”链路的当前实测基线。  
不适用范围：不替代完整字节级协议总表；不宣称多帧回传日志已完全验证。  

## 1. 真值优先级

本文件结论来自以下来源交叉核对：

1. `tmp_doc/停电单步送票回传全流程.txt`
2. `tmp_doc/停电多步传票回传全流程.txt`
3. 当前主程序已验证的 `Q_TASK / SET_COM / DEL` 协议实现
4. 同事提供的历史协议分析文档

执行原则：

1. 以真实串口样本为第一依据。
2. 历史分析文档仅作为补充，不直接视为真值。
3. 若未来多帧样本与本文冲突，以新样本修正文档与代码。

## 2. 当前已坐实的回传主链

当前单步/多步样本共同支持以下时序：

```text
钥匙放回
  -> SET_COM
  -> Q_TASK(0x04)
  -> 发现目标任务 status=0x02（操作完成）
  -> I_TASK_LOG(0x05) 请求任务操作日志
  -> 设备返回 I_TASK_LOG 响应（当前样本为总帧数=1）
  -> 主机发送 ACK(0x5A 05 00 00)
  -> 设备发送 UP_TASK_LOG(0x15)
  -> 主机解析日志并组 HTTP 回传 JSON
  -> HTTP 成功
  -> DEL(0x06) 删除钥匙内任务
```

说明：

1. 当前真实样本均为“单帧日志回传”。
2. 多帧 `UP_TASK_LOG` 尚无真机样本，不能拍板。

## 3. Q_TASK（0x04）在回传链中的语义

### 3.1 查询请求

1. `Cmd=0x04`
2. `Data=FF * 16` 表示查询钥匙上的全部任务
3. 当前主程序现有实现与实测一致

### 3.2 查询应答

任务数据格式当前可按下列方式解析：

```text
count(1B)
task[0] = taskId(16B) + status(1B) + type(1B) + source(1B) + reserved(1B)
task[1] = ...
```

当前已实测确认：

1. `status=0x02` 可视为“操作完成”，回传链会继续请求 `I_TASK_LOG`
2. `type=0x00` 在当前停电影响样本中出现
3. `source=0xFF` 在当前停电影响样本中出现

当前未完全坐实：

1. `status=0x00/0x01/0x04/0x05` 的真实行为
2. 更多 `type/source` 枚举值

## 4. I_TASK_LOG（0x05）当前基线

### 4.1 请求帧

1. `Cmd=0x05`
2. `Data=16B taskId`
3. 当前样本显示其 `Addr2` 与站号相关，表现和 `DEL` 一致

### 4.2 应答帧

当前单步/多步样本均收到：

```text
Cmd=0x05
Data=01 00
```

当前最合理解释：

1. `Data` 为日志总帧数（LE）
2. 当前两份样本都表示总帧数为 `1`

### 4.3 主机确认帧

当前真实样本里，主机不是只发 `ACK 05`，而是：

```text
7E 6C FF 00 00 00 04 00 5A 05 00 00 5C A9
```

也就是：

1. `Cmd=0x5A`
2. `Data=05 00 00`

当前解释：

1. `0x05` 表示确认 `I_TASK_LOG`
2. 后两字节当前可视为“从日志帧 0 开始上传”的确认参数

注意：

1. 该确认格式已被两份真实样本支持
2. 历史文档若只写“收到 0x05 后回复 ACK”，粒度不够，主程序实现必须按真实字节对齐

## 5. UP_TASK_LOG（0x15）当前基线

### 5.1 外层帧

当前样本表现为：

```text
Cmd=0x15
Data = frameSeq(2B LE) + filePayload(NB)
```

其中：

1. `frameSeq` 从 `0` 开始
2. 当前样本都是 `frameSeq=0`
3. 当前样本都是单帧日志

### 5.2 内层日志 payload

当前单帧 payload 结构可按下列方式解释：

```text
fileSize(4B LE)
version(2B BCD)
reserved(2B)
taskId(16B)
stationNo(1B)
taskAttr(1B)
steps(2B LE)
logItem[0](16B)
logItem[1](16B)
...
```

其中每条 `logItem` 当前可按 16B 解析：

```text
serialNumber(2B LE)
opStatus(1B)
rfid(5B)
opTime(6B BCD, yyMMddHHmmss)
opType(1B)
reserved(1B)
```

### 5.3 已被真实样本证实的字段映射

`UP_TASK_LOG -> HTTP JSON` 当前已能稳定映射：

1. `taskId(16B LE 前8字节)` -> 十进制字符串 `taskId`
2. `stationNo` -> HTTP `stationNo`
3. `steps` -> HTTP `steps`
4. `serialNumber` -> `taskLogItems[].serialNumber`
5. `opStatus` -> `taskLogItems[].opStatus`
6. `rfid(5B)` -> 去掉分隔符的大写 HEX 字符串
7. `opTime(6B)` -> `20yy-MM-dd HH:mm:ss`
8. `opType` -> `taskLogItems[].opType`

## 6. 当前已确认的样本结论

### 6.1 单步样本

来源：`tmp_doc/停电单步送票回传全流程.txt`

已确认：

1. `Q_TASK` 回包里目标任务 `status=0x02`
2. `I_TASK_LOG -> ACK(5A 05 00 00) -> UP_TASK_LOG` 主链成立
3. 解析出的单条日志与 HTTP 回传 JSON 完全对应

### 6.2 多步样本

来源：`tmp_doc/停电多步传票回传全流程.txt`

已确认：

1. `steps=2`
2. `UP_TASK_LOG` 中连续两条 `logItem`
3. 两条日志与 HTTP 回传 JSON 中的两条 `taskLogItems` 完全对应
4. `serialNumber` 分别为 `0`、`1`

### 6.3 原产品与本地程序对照结论（2026-03-10）

来源：

1. `tmp_doc/原产品_停电送电全流程单步.txt`
2. `tmp_doc/本地程序_停电送电全流程一步测试_.txt`

已共同确认：

1. 原产品与当前主程序的单步回传主链同构：
   - `Q_TASK(status=0x02) -> I_TASK_LOG -> ACK(5A 05 00 00) -> UP_TASK_LOG -> HTTP回传 -> DEL`
2. 对同一把 411 锁，工作台任务中的步骤 `rfid` 在停电/送电样本里都保持为：
   - `C9:04:99:A0:02`
3. 但回传日志 / HTTP 回传中的 `rfid` 与工作台任务中的 `rfid` **并不相同**
4. 这不是当前主程序新增的问题，因为原产品同样表现出这一特征

### 6.4 本地程序零回归补充结论（2026-03-11）

来源：

1. 本地程序真机日志 `serial_log_20260311_140842.csv`
2. 接入手工 `INIT(0x02)` / 手工 `DN_RFID(0x1A)` 后的回归验证

已确认：

1. 在本地程序中接入手工 `INIT / DN_RFID` 后，自动回传主链仍保持：
   - `Q_TASK(status=0x02) -> I_TASK_LOG -> ACK(5A 05 00 00) -> UP_TASK_LOG -> HTTP回传 -> DEL`
2. 在“什么都不点”的自动回传零回归场景里，主程序不会自动插入 `INIT / DN_RFID`
3. 因此当前应把：
   - `INIT(0x02)`
   - `DN_RFID(0x1A)`
   视为**手工链**，而不是默认 ready 自动前置链

## 7. RFID 语义补充结论（关键）

当前样本显示，至少存在两类不同语义的 RFID：

1. 工作台任务步骤中的 `rfid`
   - 由工作台/后端下发
   - 当前样本中固定表现为类似 `C9:04:99:A0:02`
2. 钥匙回传日志中的 `rfid`
   - 来自 `UP_TASK_LOG(0x15)` 的真实日志项
   - 当前样本中表现为类似 `C90499A001`、`C90499A000`

当前必须遵守的结论：

1. **不能**把工作台任务中的 `rfid` 与回传日志中的 `rfid` 当成同一个字段语义
2. **不能**因为两者不相等，就判定当前主程序回传解析错误
3. 回传 HTTP 上传时，应以 `UP_TASK_LOG(0x15)` 中解析出的日志 `rfid` 为准

## 8. 单步停电 / 送电回传差异（高可信）

原产品与当前主程序都共同表现出以下规律：

1. 单步停电样本中，回传 `rfid` 可为：
   - `C90499A001`
2. 单步送电样本中，回传 `rfid` 可为：
   - `C90499A000`

当前可做出的保守结论：

1. 回传 `rfid` **可能**与业务类型（停电 / 送电）有关
2. 这一现象已经在原产品上出现，因此不能先归因于当前主程序代码错误

当前仍不能拍死的部分：

1. 是否“所有停电都固定为 A001、所有送电都固定为 A000”
2. 该差异是否还与钥匙、锁状态、初始化状态有关

## 9. 414 锁失败的当前定性

当前联调中，414 锁出现过 HTTP 回传失败：

```text
code = 400
msg = 未找到步骤序列号为0的RFID对应的闭锁装置
```

经确认：

1. 当前联调用后端与原产品后端不是同一套
2. 当前联调用后端缺少 414 的 RFID 数据

因此当前定性应为：

1. 414 失败首先属于**后端业务数据缺失 / RFID 映射未配置**
2. 当前不应先把这类失败定性为主程序回传链错误
3. 当前联调验证应优先使用“后端已配置 RFID 的锁”

## 10. 当前不能拍板的部分

1. `UP_TASK_LOG` 多帧续传时，真实设备对每帧 ACK/NAK 的最终要求
2. 多帧情况下，最后一帧是否还需要 ACK
3. HTTP 回传失败时，原产品是否继续 `DEL`
4. `Q_TASK` 其他状态码的完整语义

## 11. 对主程序实现的直接约束

1. 回传链不能绕开现有钥匙会话状态机，仍然要建立在 `SET_COM / keyStable / sessionReady` 之上。
2. `0x05 / 0x15` 的握手细节必须放在 `features/key/protocol`，不要写进 UI。
3. `HTTP客户端报文` 页应显示“正在回传 / 发送 JSON / 收到响应”的日志，但页面本身不能直连 `QNetworkAccessManager`。
4. 当前代码已增加“按 `seq` 累积多帧 `UP_TASK_LOG`”的能力，逐帧 ACK 采用：
   - `ACK data = 0x15 + seqLo + seqHi`
5. 上述多帧 ACK 规则当前属于**基于协议文档与单帧 ACK 形式的工程推断**
6. 在没有真实多帧样本前，不应宣称该规则已被真机确认

## 12. HTTP 回传接口基线

当前已知接口信息：

1. 方法：`POST`
2. 路径：`/api/kids-outage/third-api/finish-step-batch`
3. Body：`application/json`

当前主程序实现策略：

1. 优先使用配置项 `ticket/httpReturnUrl`
2. 若未显式配置，则回退到：`system/apiUrl + "/finish-step-batch"`
3. `opTime` 当前按真实成功样本使用：`yyyy-MM-dd HH:mm:ss`

注意：

1. 历史接口文档样例里出现过 `opTime="250414155001"` 的紧凑格式。
2. 但当前两份原产品真实成功样本里，HTTP 实际发送的是完整时间串，例如：
   - `2026-03-09 13:43:41`
   - `2026-03-09 14:25:23`
3. 因此主程序当前以“真实成功样本”为准，不按紧凑 BCD 字符串发送。

## 13. 后续需要补的样本

1. 一份 `UP_TASK_LOG` 多帧样本
2. 一份 HTTP 回传失败样本
3. 一份 `Q_TASK status=0x01` 样本
4. 一份多帧日志场景下主机 ACK/NAK 的完整串口记录
5. 本地程序“单步停电 / 单步送电”的对照样本
6. 换另一把钥匙后，对同一把已配置锁做“单步停电 / 单步送电”的对照样本

## 14. 当前多帧回放验证

当前已增加 replay 场景：

1. `test/replay/scenario_up_task_log_multiframe.jsonl`

用途：

1. 验证 `I_TASK_LOG(totalFrames=2)` 后，主程序可累计两帧 `UP_TASK_LOG`
2. 验证主程序会对每帧回传日志发送 `ACK`
3. 验证累计完成后仍能组装统一的回传 HTTP JSON
