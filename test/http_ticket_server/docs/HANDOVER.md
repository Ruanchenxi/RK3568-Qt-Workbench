# http_ticket_server 测试小程序 - 完整项目交接文档

> **生成日期**：2026-03-06  
> **目标读者**：完全没有历史记录的新 Agent  
> **唯一目标**：看完本文档可立即无缝接手

---

## 一、项目定位与背景

### 1.1 这个小程序是干什么的？

`http_ticket_server` 是 RK3568 大项目的**测试前置工具**，独立于主工程，用于：

1. 拦截 RK3568 程序（`D:\Desktop\QTtest\RK3568\RK3568`）申请停电任务时发出的 HTTP POST 请求
2. 把 JSON 票据编码成原产品 X5 钥匙底座的单帧串口传票帧（协议头 `7E 6C`，长度动态）
3. 通过 Golden 工具逐字节对比验证编码器是否与原产品完全一致
4. 最终目标：把调通的编码逻辑集成回主工程，实现 RK3568 对 X5 底座的串口通信

### 1.2 在整个大项目中的位置

```
RK3568 程序（主工程）
    ↓ 用户登录 → 工作台 → 申请停电任务
    ↓ HTTP POST JSON → localhost:8888
http_ticket_server（本小程序）← 你在这里
    ↓ 编码成单帧串口报文（286B/344B/350B 等动态长度）
X5 钥匙底座（实体硬件）← 最终发送目标
```

先在本小程序把帧调通（逐字节 PASS），再集成回主工程。

### 1.3 核心目标

复刻原产品（RK3588/X5 底座）的 `7E 6C` 串口传票协议，做到**逐字节一致**（CRC 也必须匹配）。

---

## 二、关键决策历史（新 Agent 必须知道为什么这么做）

### 决策 1：taskId 后 8 字节填 `00`，不是 `FF`

- **决策内容**：payload `p[8..23]` = 前 8 字节为 uint64 小端，后 8 字节全填 `0x00`
- **为什么**：原始代码 `uint64LeBytes()` 用 `0xFF` 填充，生成帧与原产品不符 → FAIL
- **证据**：`tests/rk_20260305_golden.hex` 实测，offset [19..26] 原产品为：
  ```
  01 A0 EF C2 20 0E 2A 1C | 00 00 00 00 00 00 00 00
  ←── uint64 小端 ────────→   ←── 填 00，不是 FF ──→
  ```
- **修复位置**：`TicketPayloadEncoder.cpp` `uint64LeBytes()` 函数末尾 `append('\x00')`

---

### 决策 2：时间字段 BCD 补位规则是 `FF FF`，不是 `00 FF`

- **决策内容**：`createTime`/`planTime` 均为 8 字节 BCD，JSON 只含到分钟（12字符，如 `"202603050653"`），秒及末字节均填 `0xFF`
- **为什么**：原始代码先填 `0x00` 再填一个 `0xFF`，生成 `06 53 00 FF`，原产品实际是 `06 53 FF FF`
- **证据**：`tests/rk_20260305_golden.hex` offset [47..50] 实测：
  ```
  20 26 03 05 06 53 FF FF  ← createTime，最后两字节均 FF
  20 26 03 06 06 53 FF FF  ← planTime，同上
  ```
- **修复位置**：`TicketPayloadEncoder.cpp` `timeToBcd()` 函数，去掉 `targetLen - 1` 的判断，统一填 `0xFF`

---

### 决策 3：步骤区每步 8 字节固定结构

- **决策内容**：每个步骤 step[i] 占 8 字节，布局：
  ```
  [+0..1]  innerCode  LE uint16
  [+2]     lockerOp   uint8
  [+3]     state      uint8  = 0x00（未操作）
  [+4..7]  displayOff LE uint32（回填指向 stepDetail 字符串）
  ```
- **为什么**：从 `tests/rk_20260305_golden.hex` 反解确认，步骤区 offset=160，`01 00 02 00 A8 00 00 00`，A8=168=字符串区起始
- **证据**：Golden 工具解析结果 + `GoldenPanel.cpp` 常量 `STEP_SIZE=8`

---

### 决策 4：字符串区编码用 GBK，不是 UTF-8

- **决策内容**：`stepDetail` 和 `taskName` 均用 GBK 编码 + `0x00` 结尾
- **为什么**：原产品 HEX 中中文字符（如 `BD E2 B3 FD` = "解除"）是 GBK 编码，UTF-8 编码结果完全不同
- **证据**：`tests/rk_20260305_golden.hex` 字符串区 GBK 解码正确显示"解除锁定，将411断路器操作到隔离位置并执行锁定"

---

### 决策 5：主协议帧头是 `7E 6C`，不是 `0xAA` 等其他协议

- **决策内容**：帧固定以 `7E 6C` 开头，这是 X5 底座传票协议的魔字
- **为什么**：从原产品串口抓包直接确认，无需讨论
- **帧结构**：`7E 6C KeyId FrameNo Station Device Len_Lo Len_Hi Cmd Seq_Lo Seq_Hi [Payload] CRC_Hi CRC_Lo`

---

### 决策 6：CRC 算法为 CRC-16/CCITT，覆盖 `Cmd+Seq+Payload`，结果大端追加

- **决策内容**：CRC 初值 `0x0000`，多项式 `0x1021`，右移查表法，覆盖范围 = `Cmd(1B) + Seq(2B) + Payload`，CRC 大端写入帧尾
- **证据**：`TicketFrameBuilder.cpp` 注释：`验证向量：[0F 01] → 0x1C11 / [04 FF×16] → 0xF326`
- **Golden 验证**：`tests/rk_20260305_golden.hex` CRC = `0x206C`，与 FrameBuilder 计算一致

---

### 决策 7：Golden HEX 来源必须是原产品串口真实抓包

- **决策内容**：golden.hex 必须从原产品（RK3588）通过 VNC 登录后从串口抓包工具复制，不能手动构造
- **为什么**：Golden 的意义是"原产品真实行为基准"，手动构造的 HEX 无意义
- **操作方式**：VNC 连接原产品 → 串口监听工具 → 复制 HEX → 保存为 `tests/xxx.hex`

---

## 三、当前实现状态

### 3.1 已实现功能清单

| 功能 | 实现状态 | 实测验证 |
|------|----------|----------|
| HTTP 服务器（端口 8888）拦截 POST | ✅ | ✅ |
| JSON 自动保存到 `logs/` 目录 | ✅ | ✅ |
| 单步票 payload 编码（160B头+8B步骤+字符串区）| ✅ | ✅（停电单步 PASS）|
| 多步票 payload 编码（stepNum≥2，多步骤区 + 多字符串区）| ✅ | ✅（停电双步/三步、送电双步 PASS）|
| 时间 BCD 编码（`YYYYMMDDHHMM` → 8B，末位 FF FF）| ✅ | ✅ |
| taskId uint64-LE 编码（后 8B 填 00）| ✅ | ✅ |
| GBK 字符串编码 + 0x00 结尾 | ✅ | ✅ |
| CRC-16/CCITT 计算（覆盖 Cmd+Seq+Payload）| ✅ | ✅ |
| 帧构建（`7E 6C` 头 + payload + CRC 大端）| ✅ | ✅ |
| ReplayPanel GUI（JSON→帧 生成/复制/保存）| ✅ | ✅ |
| ReplayPanel 使用最新收到的 JSON | ✅ | ✅ |
| ReplayPanel 自动跟随最新 HTTP JSON | ✅ | ✅ |
| GoldenPanel GUI（HEX解析/生成JSON/Diff）| ✅ | ✅ |
| GoldenPanel 多步样本解析展示 | ✅ | ✅ |
| LogWindow 三Tab布局 | ✅ | ✅ |
| **auth 字段（p[48..95] 授权信息）** | ❌ 暂全FF | ❌ |
| **ticketAttr（普通票/其他类型区分）** | ❌ 暂全00 | ❌ |
| **QSerialPort 自动串口发送** | ❌ 未实现 | ❌ |

### 3.2 核心文件说明

```
test/http_ticket_server/
├── main.cpp                  # 入口，启动 QApplication + LogWindow
├── TicketHttpServer.h/.cpp   # HTTP 层：监听 8888，接收 JSON，自动保存 logs/
├── LogWindow.h/.cpp          # 主窗口：三 Tab（HTTP日志 / Replay / Golden工具）
├── TicketPayloadEncoder.h/.cpp  # 编码层核心：JSON → payload（长度动态），含全部字段
├── TicketFrameBuilder.h/.cpp    # 帧构建层：payload → 单帧串口帧（长度动态），含CRC
├── ReplayRunner.h/.cpp          # 验证层：JSON文件→帧→与Golden逐字节对比
├── ReplayPanel.h/.cpp           # GUI：🔬 Replay/Diff Tab
├── GoldenPanel.h/.cpp           # GUI：🛠 Golden工具 Tab
└── tests/
    ├── rk_20260305_golden.hex           # 停电单步 golden（286B，已 PASS）
    ├── rk_20260305_golden_replay.json   # 对应的 replay JSON（从 golden 反解）
    ├── 20260306_163854.hex              # 停电双步 golden（344B，已 PASS）
    ├── rk_20260306_real_sample.hex      # 停电双步+长 taskName 样本（350B，已 PASS）
    ├── rk_20260306_real_sample.json     # 对应真实 JSON 样本
    └── （后续继续补充停电三步 / 送电单步 / 送电双步样本）
```

**各文件核心方法：**

- `TicketPayloadEncoder::encode()` — 主编码入口，返回 `EncodeResult{payload, fieldLog}`
- `TicketPayloadEncoder::timeToBcd()` — 时间字符串 → 8B BCD，不足位填 `0xFF`
- `TicketPayloadEncoder::uint64LeBytes()` — uint64 → 8B LE，后 8B 填 `0x00`
- `TicketFrameBuilder::buildFrames()` — payload → `QList<QByteArray>` 帧列表
- `TicketFrameBuilder::calcCrc()` — CRC-16/CCITT 查表法
- `ReplayRunner::run()` — 读 JSON 文件 → 编码 → 可选 diff Golden
- `GoldenPanel::onParse()` / `onGenerateJson()` / `onDiff()` — 三按钮核心逻辑

### 3.3 代码架构分层

```
┌──────────────────── GUI 层 ─────────────────────────┐
│  LogWindow（三Tab）                                  │
│  ├── ReplayPanel：JSON文件选择 → 生成/对比/复制/保存  │
│  └── GoldenPanel：HEX文件选择 → A解析/B生成JSON/C对比 │
└───────────────────────────────────────────────────────┘
            ↓
┌──────────────────── 验证层 ─────────────────────────┐
│  ReplayRunner：协调 编码层 + 帧层 + 对比逻辑         │
└───────────────────────────────────────────────────────┘
            ↓
┌──────────────────── 编码层 ─────────────────────────┐
│  TicketPayloadEncoder：JSON → payload（273B）         │
└───────────────────────────────────────────────────────┘
            ↓
┌──────────────────── 帧构建层 ───────────────────────┐
│  TicketFrameBuilder：payload → 完整串口帧（286B）    │
│  含 CRC-16/CCITT 计算                               │
└───────────────────────────────────────────────────────┘
            ↓
┌──────────────────── HTTP 层 ────────────────────────┐
│  TicketHttpServer：监听 8080，保存 JSON 到 logs/     │
└───────────────────────────────────────────────────────┘
```

---

## 四、串口协议最终版（从代码实测确认）

### 4.1 通信参数

代码中未限定波特率（由串口助手/硬件决定），帧格式由 `TicketFrameBuilder` 固定。

### 4.2 完整帧结构（单帧，长度动态）

```
字节    字段          长度  值/规则
[0]     SOF_0         1B    0x7E（固定）
[1]     SOF_1         1B    0x6C（固定）
[2]     KeyId         1B    0x00
[3]     FrameNo       1B    跨调用累加，初始 0x00
[4]     Station       1B    stationId（一般 0x01）
[5]     Device        1B    0x00
[6..7]  Len           2B    LE，= Cmd(1)+Seq(2)+PayloadLen
[8]     Cmd           1B    0x03（单帧/末帧=TICKET），多帧中间帧=0x83
[9..10] Seq           2B    LE，帧序号从 0 开始
[11..]  Payload       nB    见 Payload 结构
[尾-2]  CRC_Hi        1B    大端高字节
[尾-1]  CRC_Lo        1B    大端低字节
```

已验证单帧长度示例：

- 单步票：286B（payloadLen=273，Len=276）
- 双步票：344B（payloadLen=331，Len=334）
- 双步长任务名：350B（payloadLen=337，Len=340）

### 4.3 Payload 结构（273B，单步票）

```
payload偏移  字段             长度  编码规则
[0..3]       fileSize         4B    LE uint32，= payload总长（273 = 0x111），最后回填
[4..5]       version          2B    固定 01 11
[6..7]       reserved         2B    固定 00 00
[8..23]      taskId           16B   前8B = uint64 LE（JSON taskId字段），后8B = 全00
[24]         stationId        1B    = stationId参数（0x01）
[25]         ticketAttr       1B    0x00（TODO：普通票/其他类型未区分）
[26..27]     stepNum          2B    LE uint16，= 步骤数
[28..31]     taskNameOff      4B    LE uint32，指向 taskName 字符串的 payload 内偏移（回填）
[32..39]     createTime       8B    BCD，YYYYMMDDHHMM共6字节，后2字节填 FF
[40..47]     planTime         8B    同上
[48..95]     auth             48B   全 FF（TODO：授权信息未实现）
[96..127]    addFields        32B   全 FF（TODO：跳步码偏移等未实现）
[128..159]   reserved2        32B   全 FF
[160..161]   step[0].innerCode 2B   LE uint16
[162]        step[0].lockerOp  1B   uint8
[163]        step[0].state     1B   0x00（未操作）
[164..167]   step[0].displayOff 4B  LE uint32，指向 stepDetail 字符串的 payload 内偏移（回填）
[168..213]   stepDetail        46B  GBK编码字符串 + 0x00结尾
[214..272]   taskName          59B  GBK编码字符串 + 0x00结尾
```

### 4.4 CRC 算法

```cpp
// 算法：CRC-16/CCITT，右移查表，初值 0x0000，多项式 0x1021
// 覆盖范围：Cmd(1B) + Seq(2B) + Payload(273B) = 276 字节
// 结果写法：大端，先写 CRC_Hi 再写 CRC_Lo
// 验证向量（代码注释）：[0F 01] → 0x1C11  /  [04 FF×16] → 0xF326
// Golden 验证：rk_20260305_golden.hex CRC = 0x206C（已实测一致）
```

---

## 五、实测验证记录

### 5.1 已验证通过

**单步票 Golden PASS（`rk_20260305_golden.hex`）**

- 任务：TD-2026-03-05-000006-设备检修-大横琴B2低压配电房-411断路器
- taskId：2029450115956056065
- stepDetail：解除锁定，将411断路器操作到隔离位置并执行锁定
- createTime：2026-03-05 06:53 / planTime：2026-03-06 06:53
- 帧长：286B，CRC：0x206C
- 结论：编码器与原产品逐字节一致，Golden 对比 ✅ PASS

**停电双步 Golden PASS（`tests/20260306_163854.hex`）**

- stepNum：2
- 帧长：344B
- payloadLen/fileSize：331（0x014B）
- Len：334（0x014E）
- taskNameOff：268（0x010C）
- displayOff[0]：176（0x00B0）
- displayOff[1]：222（0x00DE）
- CRC：0xE31F
- 结论：双步票编码与原产品逐字节一致，Golden 对比 ✅ PASS

**停电双步 + 长 taskName 样本 PASS（`tests/rk_20260306_real_sample.hex`）**

- stepNum：2
- 帧长：350B
- payloadLen/fileSize：337（0x0151）
- Len：340（0x0154）
- taskNameOff：268（0x010C）
- displayOff[0]：176（0x00B0）
- displayOff[1]：222（0x00DE）
- CRC：0x5828
- 结论：动态长度 taskName 场景已验证 PASS

**停电三步样本 PASS**

- stepNum：3
- 帧长：412B
- 结论：三步步骤区 + 三段 stepDetail + taskName 的动态偏移与长度已验证 PASS

**送电单步样本 PASS**

- stepNum：1
- 帧长：266B
- 结论：送电场景（`taskType=1`、`lockerOperate=3`）单步样本已验证 PASS

**送电双步样本 PASS**

- stepNum：2
- 帧长：310B
- 结论：送电场景双步样本已验证 PASS

**已验证规则（当前阶段）**

- `stepTableStart = p[160]`
- 每步 step record 固定 8B：
  `innerCode(2B LE) + lockerOp(1B) + state(1B=00) + displayOff(4B LE)`
- `stringAreaStart = 160 + 8 * stepNum`
- `displayOff[i] / taskNameOff / fileSize / Len / CRC` 均动态回填

### 5.2 验证失败但已解决

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| FAIL：offset [19..24] taskId 不符 | `uint64LeBytes()` 用 `0xFF` 填充后8B | 改为 `0x00` |
| FAIL：offset [47..48] createTime 秒位不符 | `timeToBcd()` 秒位填 `0x00` | 统一填 `0xFF` |
| 编译报错：`QByteArray::toHex()` 传给 `QString::arg()` 歧义 | Qt 重载冲突 | 用 `QString::fromLatin1(...toHex(...))` 包装 |
| 编译报错：`m_goldenPanel` 未声明 | `LogWindow.h` 格式问题 | 修复成员声明排版 |
| 编译失败：`onClearClicked()` 重复定义 | `LogWindow.cpp` 中同名函数两个 | 删除重复定义 |

### 5.3 尚未验证的功能

- **自动串口发送**：目前只能复制 HEX 手动粘贴到串口助手
- **硬件 ACK 响应**：尚未通过串口线连接 X5 底座实机验证
- **多帧传输**：payload > 512B 时会分帧，代码有实现但未测试
- **auth 字段**：p[48..95] 全 FF，实际含义未解析

### 5.4 当前 GUI 能力

- Replay / Diff Tab 支持“使用最新收到的 JSON”
- Replay / Diff Tab 支持“自动跟随最新 HTTP JSON”
- Golden 工具 Tab 支持按 `stepNum` 循环展示全部 `step[i]`
- Golden 工具 Tab 文案已改为通用单帧传票帧描述，不再写死 286B

### 5.5 当前 GUI / 工具边界

- 还未集成串口发送，仍需手动复制 HEX 到串口助手
- ACK 回包尚未纳入自动验证闭环
- 多帧 `0x83` 逻辑有代码但未完成真实样本验证
- `auth / rfid / stepId / stepType` 仍未进入 payload 精确映射

---

## 六、已知问题与待办清单

### 6.1 代码中的 TODO

| 位置 | TODO 内容 |
|------|-----------|
| `TicketPayloadEncoder.cpp` L202 | `[48..95] auth` 暂全 FF，授权信息未实现 |
| `TicketPayloadEncoder.cpp` L206 | `[96..127] addFields` 暂全 FF，跳步码偏移等未实现 |
| `TicketPayloadEncoder.cpp` L169 | `ticketAttr` 普通票/其他类型未区分，固定 0x00 |
| `TicketPayloadEncoder.cpp` L159 | taskId 非纯数字时有 `[TODO: taskId非纯数字]` 警告 |
| `GoldenPanel.cpp` 常量注释 | `P_TASKID` 注释写"rest FF"但实际应为"rest 00"，已修复代码但注释未更新 |

### 6.2 待开发任务（按优先级）

**P0：继续批量样本验证**
- **要做什么**：继续采集并验证新的停电/送电真实样本
- **为什么**：当前编码器已经通过停电单步、停电双步（344B/350B）、停电三步、送电单步、送电双步，下一阶段应继续以真实样本回放为主，而不是预先改协议
- **验收标准**：每抓到一条样本，都能做到 JSON + golden HEX 的 Replay/Diff；只有 FAIL 时才改编码逻辑

**P1：QSerialPort 自动发送 + ACK 校验**
- **要做什么**：在 Replay Tab 增加串口选择下拉框，点"发送"直接串口发帧，等待 X5 底座 ACK
- **为什么**：目前需要手动复制 HEX 到串口助手，效率低
- **验收标准**：程序内直接发帧并收到 X5 底座 ACK 响应

**P2：集成回主工程**
- **要做什么**：把 `TicketPayloadEncoder` + `TicketFrameBuilder` 移植到 `D:\Desktop\QTtest\RK3568\RK3568` 主工程
- **为什么**：本小程序是测试工具，最终逻辑要跑在主工程上
- **验收标准**：主工程申请任务后自动编码并通过串口发给 X5 底座

**P3：auth 字段实现**
- **要做什么**：解析 p[48..95] 授权信息的真实结构，从 JSON 或 golden 反解
- **为什么**：全 FF 可能与原产品不符，真机测试时可能被底座校验拒绝

---

## 七、避坑指南

### 坑 1：Replay Tab 里选择 Golden 文件对比，任务不一样必然 FAIL

- **坑**：Replay Tab 的 JSON 是今天申请的新任务，Golden HEX 是另一条老任务，taskId/时间完全不同，对比永远 FAIL
- **发现**：对比日志首处差异 `offset=19 生成=02 期望=01`（taskId 不同）
- **正确用法**：
  - Replay Tab：只用于"日常生成帧发给硬件"，**不要拿新任务 JSON 和旧任务 Golden 对比**
  - Golden 工具：A→B→C 全用同一个 HEX 文件，自给自足验证编码器

### 坑 2：改了代码没重新编译，还是看到旧版本的 FAIL

- **坑**：`TicketPayloadEncoder.cpp` 改了两处，但程序还在运行旧 exe，diff 结果不变
- **发现**：FAIL 日志和修复前完全一样
- **解决**：停止运行中的 exe → 执行 `mingw32-make -j4` → 重新运行新 exe
- **编译命令**：
  ```powershell
  Stop-Process -Name "http_ticket_server" -ErrorAction SilentlyContinue
  cd 'D:\Desktop\QTtest\RK3568\test\http_ticket_server\build\release'
  mingw32-make -j4
  ```

### 坑 3：VNC 复制 JSON 中文乱码，不能直接用

- **坑**：VNC 连接原产品后复制的 JSON 中文字段是乱码（原产品用 GBK，VNC 剪贴板 UTF-8）
- **解决**：不要从 VNC 复制 JSON，**通过 Golden 工具从串口 HEX 反解 JSON**（A→B按钮）

### 坑 4：Golden 工具里 C 按钮 FAIL 时，先看哪个字段不同

排查顺序：
1. **taskId（offset 19~26）**：检查 JSON 里 taskId 是否和 HEX 里一致
2. **时间（offset 43~58）**：检查 BCD 秒位，应是 `FF FF` 不是 `00 FF`
3. **字符串区（offset 170+）**：检查 GBK 编码是否正确，特别是中文字符
4. **CRC（offset 284~285）**：CRC 差异是前面字段不同的结果，不要单独修 CRC

### 坑 5：exe 在 `build\release\release\` 双层 release 目录

- **坑**：找不到编译出的 exe
- **实际路径**：`D:\Desktop\QTtest\RK3568\test\http_ticket_server\build\release\release\http_ticket_server.exe`
- **原因**：qmake spec `win32-g++` + Release 配置下的双层目录结构

### 坑 6：不要拿旧结论覆盖当前已验证规则

- **坑**：早期文档里把多步票写成“未实现”或把 `displayOff` 认为是写死 `0xA8`
- **当前真实情况**：多步票已经通过 2 步和 3 步样本验证，`stringAreaStart / displayOff[i] / taskNameOff / fileSize / Len / CRC` 都是动态回填
- **正确做法**：后续遇到新样本时，先 Replay/Diff 验证，只有 FAIL 才改编码逻辑

---

## 八、如何使用这个项目

### 8.1 如何编译

```powershell
# 环境要求：Qt 5.15.2 mingw81_64，D:\Qt\5.15.2\mingw81_64
$env:PATH = 'D:\Qt\Tools\mingw810_64\bin;' + $env:PATH

# 如果 build 目录不存在，先执行 qmake
New-Item -ItemType Directory -Force 'D:\Desktop\QTtest\RK3568\test\http_ticket_server\build\release'
cd 'D:\Desktop\QTtest\RK3568\test\http_ticket_server\build\release'
D:\Qt\5.15.2\mingw81_64\bin\qmake.exe '..\..\http_ticket_server.pro' -spec win32-g++ CONFIG+=release

# 编译
mingw32-make -j4
# exe 输出位置：build\release\release\http_ticket_server.exe
```

### 8.2 如何运行

1. 运行 `build\release\release\http_ticket_server.exe`
2. 程序自动在 `8888` 端口启动 HTTP 服务器
3. 运行 `D:\Desktop\QTtest\RK3568\RK3568` 主工程，登录 → 工作台 → 申请停电任务
4. HTTP 日志 Tab 会自动显示收到的 JSON，并保存到 `build\release\release\logs\` 目录
5. Replay / Diff Tab 可一键切换到“最新收到的 HTTP JSON”，也可启用自动跟随

### 8.3 如何做 Golden 验证

1. 从原产品（VNC 连接）的串口监听工具复制 HEX 报文，保存为 `tests/xxx_golden.hex`（空格分隔格式）
2. 打开 `http_ticket_server.exe` → **🛠 Golden 工具** Tab
3. 点"选择…"选择 `xxx_golden.hex`
4. 点 **A（解析 golden）**：显示所有字段的解析结果，确认读取正确
5. 点 **B（生成 replay.json）**：从解析字段生成 JSON 文件
6. 点 **C（生成帧并 diff）**：用 B 生成的 JSON 重新编码，与原始 HEX 逐字节对比
7. 输出 `✅ PASS` 表示编码器与原产品一致

### 8.3.1 推荐验证流程（当前阶段）

1. 每测一条业务，优先保存三样：
   - 原产品 HTTP JSON
   - 原产品串口 HEX
   - ACK 回包（如果能抓到）
2. 回到 `http_ticket_server.exe`：
   - 在 Replay / Diff Tab 里选 JSON（或使用“最新收到的 JSON”）
   - 选对应 golden HEX
   - 做 Replay / Diff
3. 如果 PASS：
   - 不改编码逻辑
4. 如果 FAIL：
   - 只针对首个差异字段定位问题，再修改编码逻辑

### 8.4 如何添加新的测试样本

1. 将新 HEX 文件保存到 `tests/` 目录，文件名格式：`YYYYMMDD_HHMMSS.hex`
2. HEX 格式：每字节两位十六进制 + 空格分隔，可以有换行（`GoldenPanel` 自动处理）
3. 用 Golden 工具 A→B→C 步骤验证
4. 当前多步票已支持；新样本优先走 Replay/Diff 验证，只有 FAIL 才改代码

---

## 附录：关键报文样本

### 单步票 Golden（已 PASS）

文件：`tests/rk_20260305_golden.hex`

```
7E 6C 00 00 01 00 14 01 03 00 00 11 01 00 00 01
11 00 00 01 A0 EF C2 20 0E 2A 1C 00 00 00 00 00
00 00 00 01 00 01 00 D6 00 00 00 20 26 03 05 06
53 FF FF 20 26 03 06 06 53 FF FF FF...（中略）...
...01 00 02 00 A8 00 00 00 BD E2 B3 FD...（stepDetail GBK）...
...54 44 2D 32 30 32 36...（taskName GBK）...00 20 6C
```

CRC = `0x206C`，帧长 286B

### 两步票 Golden（已验证 PASS）

文件：`tests/20260306_163854.hex`

```
7E 6C 00 00 01 00 4E 01 03 00 00 4B 01 00 00 01
...stepNum=02...03 00 02 00 B0 00 00 00...（step[0]）
...04 00 02 00 DE 00 00 00...（step[1]）
```

CRC = `0xE31F`，帧长 344B，stepNum=2

### 停电双步 + 长 taskName 样本（已 PASS）

文件：`tests/rk_20260306_real_sample.hex`

- 帧长：350B
- payloadLen/fileSize：337
- Len：340
- CRC：0x5828
