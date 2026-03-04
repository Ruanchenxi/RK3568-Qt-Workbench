# X5 智能钥匙柜串口协议逐字节参考

Status: Active  
Owner: 钥匙链路维护者  
Last Updated: 2026-03-04  
真值来源：`KeyProtocolDefs.h` / `KeySerialClient.h` / `KeyCrc16.h` + 2026-03-03 真机抓包验证

---

## §1 通用帧格式

```text
字节偏移  长度   字段        说明
──────────────────────────────────────────────────────────
0~1       2B    帧头        固定 7E 6C
2         1B    KeyId       钥匙柜编号，FF = 广播查询所有
3         1B    FrameNo     发送序号，每次自增（0x00→0xFF循环）
4~5       2B    Addr2       扩展地址，字节序 Lo first（LE）
6~7       2B    Len         = Cmd(1) + Data(N) 的字节数，LE 小端
8         1B    Cmd         命令码（见 §2）
9~8+N     NB    Data        命令负载（N = Len - 1）
9+N~10+N  2B    CRC16       校验 Cmd+Data，BE 大端（高字节在前）
──────────────────────────────────────────────────────────
最小帧长 = 11 字节（Data 为 0 字节时）
最大 Data = 514 字节（协议规定，超出视为 Len 异常丢弃）
```

### 字节序规则

| 字段   | 字节序 | 写入方式             |
|--------|--------|----------------------|
| Len    | LE     | `Lo, Hi`             |
| Addr2  | LE     | `Lo, Hi`             |
| CRC16  | BE     | `Hi, Lo`（高字节先发）|

---

## §2 命令码一览

| 命令码 | 名称       | 方向           | 说明               |
|--------|------------|----------------|--------------------|
| 0x0F   | SET_COM    | TX（主发）     | 握手命令           |
| 0x04   | Q_TASK     | TX / RX        | 查询任务列表       |
| 0x06   | DEL        | TX（主发）     | 删除指定任务       |
| 0x5A   | ACK        | RX（设备回）   | 确认响应           |
| 0x00   | NAK        | RX（设备回）   | 拒绝/错误响应      |
| 0x11   | KEY_EVENT  | RX（设备推送） | 钥匙在位/离位事件  |

---

## §3 CRC-16 算法

### 参数

| 项目     | 值                        |
|----------|---------------------------|
| 算法变种 | CRC-16/右移查表（非CCITT左移）|
| 初值     | 0x0000                    |
| 多项式   | 0x1021（本变种）          |
| 更新公式 | `crc = (crc >> 8) ^ table[(byte ^ (crc & 0xFF)) & 0xFF]` |
| 校验范围 | **仅 Cmd + Data**（不含帧头/KeyId/FrameNo/Addr2/Len）|
| 发送字节序 | BE：高字节先发            |

⚠️ 禁止替换为 CRC-16/CCITT 左移变种，两者对相同输入产生不同结果。

### 已验证向量（2026-03-03 真机抓包 PASS）

| 输入数据（Cmd + Data）    | CRC    | 帧示例        |
|---------------------------|--------|---------------|
| `0F 01`                   | 1C11   | SET_COM 发送  |
| `04 FF×16`                | F326   | Q_TASK 发送   |
| `11 01 00 00 00`          | D162   | KEY_PLACED    |
| `11 00 00 00 00`          | 44A0   | KEY_REMOVED   |
| `5A 0F`                   | A720   | ACK(SET_COM)  |
| `5A 06`                   | 3609   | ACK(DEL)      |
| `04 00`                   | D14C   | Q_TASK resp count=0 |

---

## §4 各命令帧逐字节拆解

### §4.1 SET_COM 发送帧

```text
偏移  值    字段
0     7E    帧头[0]
1     6C    帧头[1]
2     FF    KeyId（广播）
3     00    FrameNo（首帧为0，后续自增）
4     00    Addr2 Lo
5     00    Addr2 Hi  → Addr2 = 0x0000
6     02    Len Lo
7     00    Len Hi    → Len = 0x0002（Cmd1 + Data1）
8     0F    Cmd = SET_COM
9     01    Data（握手参数）
10    1C    CRC Hi
11    11    CRC Lo    → CRC = 0x1C11
```

实测完整帧：`7E 6C FF 00 00 00 02 00 0F 01 1C 11`

---

### §4.2 ACK(SET_COM) 响应帧

```text
偏移  值    字段
0     7E    帧头[0]
1     6C    帧头[1]
2     **    KeyId（设备实际 ID，非广播）
3     **    FrameNo（设备回复序号）
4     00    Addr2 Lo
5     00    Addr2 Hi
6     02    Len Lo
7     00    Len Hi    → Len = 0x0002
8     5A    Cmd = ACK
9     0F    Data = 被确认的命令码（SET_COM）
10    A7    CRC Hi
11    20    CRC Lo    → CRC = 0xA720
```

---

### §4.3 Q_TASK 发送帧

```text
偏移  值      字段
0     7E      帧头[0]
1     6C      帧头[1]
2     FF      KeyId
3     **      FrameNo
4     00      Addr2 Lo
5     00      Addr2 Hi  → Addr2 = 0x0000
6     11      Len Lo
7     00      Len Hi    → Len = 0x0011（Cmd1 + Data16）
8     04      Cmd = Q_TASK
9~24  FF×16   Data（16 字节全 FF，通配查询所有任务）
25    F3      CRC Hi
26    26      CRC Lo    → CRC = 0xF326
```

---

### §4.4 Q_TASK 响应帧（count=0）

```text
偏移  值    字段
8     04    Cmd = Q_TASK
9     00    Data = count（任务数为 0）
10    D1    CRC Hi
11    4C    CRC Lo    → CRC = 0xD14C
```

---

### §4.5 Q_TASK 响应帧（count=N，N≥1）

```text
偏移       长度    字段
8          1B      Cmd = 0x04
9          1B      count（任务条数）
10~29      20B     任务[0]：taskId(16B) + status(1B) + type(1B) + source(1B) + reserved(1B)
30~49      20B     任务[1]（若 count≥2）
...
Len = 1(Cmd) + 1(count) + count×20
每条任务 = 20 字节固定长度
```

任务字段含义：

| 字段     | 长度 | 含义                         |
|----------|------|------------------------------|
| taskId   | 16B  | GUID 二进制形式（用于 DEL）  |
| status   | 1B   | 任务状态（协议定义）         |
| type     | 1B   | 任务类型（协议定义）         |
| source   | 1B   | 任务来源（协议定义）         |
| reserved | 1B   | 保留字段（当前未使用）       |

示例（count=1）：`Len = 0x0016`（十进制22 = 1+1+20）

---

### §4.6 DEL 发送帧

```text
偏移   值       字段
4      01       Addr2 Lo  ← ⚠️ DEL 专用，与 SET_COM/Q_TASK 不同
5      00       Addr2 Hi  → Addr2 = 0x0001
6      11       Len Lo
7      00       Len Hi    → Len = 0x0011（Cmd1 + Data16）
8      06       Cmd = DEL
9~24   taskId   Data（16 字节 GUID，来自 Q_TASK 响应）
25~26  **       CRC（随 taskId 变化）
```

参考：taskId 全 0 时 CRC = `1A8C`

---

### §4.7 ACK(DEL) 响应帧

```text
8     5A    Cmd = ACK
9     06    Data = 被确认的命令码（DEL）
10    36    CRC Hi
11    09    CRC Lo    → CRC = 0x3609
```

---

### §4.8 KEY_EVENT 放上帧（KEY_PLACED）

```text
偏移  值    字段
8     11    Cmd = KEY_EVENT
9     01    Data[0] = 01（在位标志）
10    00    Data[1]
11    00    Data[2]
12    00    Data[3]
13    D1    CRC Hi
14    62    CRC Lo    → CRC = 0xD162
Len = 0x0005（Cmd1 + Data4）
```

---

### §4.9 KEY_EVENT 拿走帧（KEY_REMOVED）

```text
偏移  值    字段
8     11    Cmd = KEY_EVENT
9     00    Data[0] = 00（离位标志）
10    00    Data[1]
11    00    Data[2]
12    00    Data[3]
13    44    CRC Hi
14    A0    CRC Lo    → CRC = 0x44A0
Len = 0x0005
```

---

### §4.10 NAK 响应帧

```text
8     00    Cmd = NAK
9     **    origCmd（被拒绝的命令码）
10    **    errCode（见错误码表）
Len 按实际 Data 长度，不能写死 2 字节
```

NAK 错误码：

| 错误码 | 含义         |
|--------|--------------|
| 0x03   | CRC 校验失败 |
| 0x04   | 长度不匹配   |
| 0x08   | GUID 不存在  |

---

## §5 完整时序图

```text
主机                                        设备
│                                           │
│──── connectPort ────────────────────────►│
│                                           │
│──── SET_COM (0x0F, Data=01) ────────────►│
│◄─── ACK(0x5A, Data=0F) ─────────────────│  sessionReady=true
│                                           │
│           （钥匙插入）                    │
│◄─── KEY_EVENT(0x11, Data=01 00 00 00) ──│  sessionReady=false
│     │                                     │  稳定性窗口启动(600ms)
│     │ 延迟600ms                           │
│──── SET_COM (第二次握手) ───────────────►│
│◄─── ACK(0x5A, Data=0F) ─────────────────│  sessionReady=true
│     稳定性窗口到期(600ms无噪声)           │
│     keyStable=true                        │
│                                           │
│──── Q_TASK (0x04, Data=FF×16) ─────────►│
│◄─── Q_TASK resp (count=N, tasks...) ────│  tasksUpdated 信号
│                                           │
│──── DEL (0x06, Data=taskId) ────────────►│
│◄─── ACK(0x5A, Data=06) ─────────────────│
│──── Q_TASK（验证删除结果）──────────────►│
│◄─── Q_TASK resp (count=N-1) ────────────│
│                                           │
│           （钥匙拔出）                    │
│◄─── KEY_EVENT(0x11, Data=00 00 00 00) ──│  keyPlaced/keyStable/sessionReady 全 false
│                                           │
```

---

## §6 拆包算法伪代码（含 resync）

```text
while buf.len() >= MIN_FRAME_LEN(11):
    if buf[0] != 0x7E or buf[1] != 0x6C:
        # resync：丢弃 1 字节，滑窗搜索下一个帧头
        buf.removeFirst(1)
        resyncDropBytes++
        continue

    lenField = buf[6] | (buf[7] << 8)   # LE 小端
    if lenField < 1 or lenField > 515:   # Cmd(1)+Data(0~514)
        # Len 异常，丢弃帧头继续 resync
        buf.removeFirst(1)
        lenInvalidCount++
        continue

    totalFrameLen = 9 + lenField + 2     # Header(2)+KeyId+FrameNo+Addr2(2)+Len(2)+Cmd(1)+Data(N)+CRC(2)
    if buf.len() < totalFrameLen:
        break   # 数据不够，等待更多数据

    frame = buf[0 .. totalFrameLen]
    cmdPlusData = buf[8 .. 8+lenField]
    actualCrc   = calc_crc16(cmdPlusData)
    frameCrc    = (buf[totalFrameLen-2] << 8) | buf[totalFrameLen-1]  # BE

    if actualCrc != frameCrc:
        buf.removeFirst(1)   # CRC 失败，滑窗 1 字节
        crcFailCount++
        continue

    # 合法帧，提取并消费
    outFrame = frame
    buf.removeFirst(totalFrameLen)
    return FRAME_OK
```

---

## §7 关键常量速查卡

| 常量名                       | 值   | 含义                    |
|------------------------------|------|-------------------------|
| FrameHeader                  | 7E 6C | 帧头                   |
| ReqKeyId                     | 0xFF | 广播 KeyId              |
| CmdSetCom                    | 0x0F | 握手命令                |
| CmdQTask                     | 0x04 | 查询任务                |
| CmdDel                       | 0x06 | 删除任务                |
| CmdAck                       | 0x5A | 确认响应                |
| CmdNak                       | 0x00 | 拒绝响应                |
| CmdKeyEvent                  | 0x11 | 钥匙事件                |
| Addr2 (SET_COM/Q_TASK)       | 0x0000 | Lo=00 Hi=00          |
| Addr2 (DEL)                  | 0x0001 | Lo=01 Hi=00          |
| RETRY_TIMEOUT_MS             | 1000 | 单次重试超时(ms)        |
| MAX_RETRIES                  | 3    | 最大重试次数            |
| KEY_STABLE_WINDOW_MS         | 600  | 稳定性观察窗口(ms)      |
| KEY_QUIET_WINDOW_MS          | 400  | 静默等待时间(ms)        |
| KEY_PLACED_HANDSHAKE_DELAY_MS| 600  | 放上后延迟握手(ms)      |
| STARTUP_PROBE_DELAY_MS       | 200  | 启动探测延迟(ms)        |
| QTASK_RECOVERY_DELAY_MS      | 500  | Q_TASK 软超时重试延迟(ms)|
| QTASK_RECOVERY_ROUNDS        | 2    | Q_TASK 软超时最大重试轮 |
| TASK_INFO_LEN                | 20   | 单条任务字节数          |
| TASK_ID_LEN                  | 16   | 任务 ID 字节数          |
| MAX_REOPEN_RETRIES           | 5    | 最大重连次数            |

---

## §8 常见易错对照表

| 错误做法                              | 正确做法                                 |
|---------------------------------------|------------------------------------------|
| CRC 用左移变种（CCITT）              | 用右移查表，更新：`(crc>>8)^table[(b^crc&0xFF)&0xFF]` |
| SET_COM CRC = 0x30C4                  | SET_COM CRC = **0x1C11**（已验证）       |
| Q_TASK Data 全 00                     | Q_TASK Data 全 **FF×16**（通配查询）     |
| DEL Addr2 = 0x0000                    | DEL Addr2 = **0x0001**（Lo=01 Hi=00）   |
| Len 含帧头/KeyId 等所有字节           | Len = Cmd(1) + Data(N)，**不含帧头以前** |
| CRC 字段 LE 小端发送                  | CRC **BE 大端**，高字节先发              |
| 用整帧字节相等判断命令匹配           | 应只比较 Cmd 字节，FrameNo 是可变的      |
| KEY_PLACED Data[0]=0x02               | KEY_PLACED Data[0]=**0x01**，拿走=0x00  |
| 以为 execute(SetCom) 会发 SET_COM 帧 | 当前该 case 只做 disconnectPort（已知 gap）|
