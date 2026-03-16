# Agent 接手文档（2026-03-13）

Status: Active  
Owner: Codex  
Last Updated: 2026-03-16  
适用范围：给“没有任何历史记录的新 agent”接手当前这轮板端稳定性修复。  
不适用范围：不替代逐字节协议文档，不替代完整架构入口。  

## 1. 当前任务背景

当前任务不是从零开发，而是在已有主链基础上，继续处理：

1. RK3588 Ubuntu 板端 UI 明显卡顿。  
2. Linux 串口时序脏，`waitForBytesWritten()` / `SerialError(12)` 频繁与合法响应并存。  
3. 自动回传链在 `I_TASK_LOG -> ACK -> UP_TASK_LOG` 一段不稳定。  
4. 顶部通讯状态与真实业务可用性不一致。  

当前已明确：

1. 主链已存在：
   - `工作台 JSON -> 本地 HTTP 接收 -> 系统票入池 -> 手动/自动传票 -> Q_TASK(status=0x02) -> I_TASK_LOG -> UP_TASK_LOG -> HTTP回传 -> DEL`
2. Windows/PC + 好底座可以跑部分链路。  
3. RK3588 板端更容易出现：
   - 已有合法响应后仍 timeout/retry
   - 自动回传卡中间态
   - HTTP 成功后钥匙任务不稳定清掉
4. 刚刚一次 Windows “未插/待钥匙、无 SET_COM ACK” 现象，后经重新拔插串口/底座恢复，暂不直接定性为代码回归。  

## 2. 本轮已经做过的代码修改

### 2.1 Linux 图形启动与 UI 减负

已修改：

1. `src/app/main.cpp`
2. `src/features/workbench/ui/workbenchpage.h`
3. `src/features/workbench/ui/workbenchpage.cpp`
4. `src/features/log/ui/logpage.h`
5. `src/features/log/ui/logpage.cpp`
6. `src/features/key/ui/keymanagepage.h`
7. `src/features/key/ui/keymanagepage.cpp`

当前已落地：

1. Linux 启动前：
   - 已改为输出图形诊断日志，但默认不再强制 `QT_XCB_GL_INTEGRATION=xcb_egl`
   - 默认 `RK3568_LINUX_GL_MODE=auto`，只在显式设置 `egl/gles/xcb_egl_gles` 时才启用 `AA_UseOpenGLES`
   - 支持 `RK3568_LINUX_GL_MODE=software` 显式切换软件 OpenGL
2. `WorkbenchPage`
   - 不在构造函数里立即创建 `QWebEngineView`
   - 首次 `showEvent()` 才初始化
   - 默认关闭高频 web debug，需 `RK3568_WEB_DEBUG=1` 才输出
3. `LogPage`
   - Linux + `service/startMode=remote` 时首显再 `start()`
   - 隐藏时日志先缓存在内存，不立即刷控件
4. `KeyManagePage`
   - 串口日志改成定时合批刷新
   - HTTP 文本和重表格改成 dirty 后合批刷新
   - `httpClientLogText/httpServerLogText` 增加 block count 上限

### 2.2 HTTP 回传收口

已修改：

1. `src/features/key/application/TicketReturnHttpClient.h`
2. `src/features/key/application/TicketReturnHttpClient.cpp`

当前已落地：

1. 使用 `QTimer + reply->abort()` 做超时控制，默认 10s。  
2. 记录 resolved URL 和 HTTP status。  
3. 成功判定更严格：
   - 必须 2xx
   - body 必须能解析为 JSON object
   - 且 `success == true` 或 `code == 200`
4. 2xx 但非 JSON object 不再默认视为成功。  

### 2.3 回传状态收口

已修改：

1. `src/features/key/application/KeyManageController.h`
2. `src/features/key/application/KeyManageController.cpp`
3. `src/features/key/ui/keymanagepage.cpp`

当前已落地：

1. `SET_COM / I_TASK_LOG / UP_TASK_LOG` timeout/NAK 会释放活动回传上下文。  
2. 新增回传状态：
   - `return-upload-success`
   - `return-delete-pending`
   - `return-delete-success`
3. `handleReturnUploadSucceeded()` 不再直接写 `return-success`，而是：
   - `return-upload-success`
   - `return-delete-pending`
   - 然后发 `DEL`
4. UI 已同步这些状态的文案和按钮规则。  

### 2.4 串口状态机 P0 止血

已修改：

1. `src/shared/contracts/IKeySessionService.h`
2. `src/features/key/application/KeySessionService.cpp`
3. `src/features/key/protocol/KeySerialClient.h`
4. `src/features/key/protocol/KeySerialClient.cpp`
5. `src/features/key/ui/keymanagepage.cpp`

当前已落地：

1. `KeySessionSnapshot` 新增：
   - `lastBusinessSuccessMs`
   - `lastProtocolFailureMs`
   - `recoveryWindowActive`
2. `KeyManagePage::updateCommIndicators()` 改成“最近成功业务响应优先”的顶部状态模型。  
3. `KeySerialClient` 当前已做的 P0：
   - 引入 `beginInFlightContext()`
   - 增加成功/失败时间戳
   - `SerialError(12)`（TimeoutError）降为 WARN，不再按硬错误恢复链处理
   - `stale retry timeout` 忽略
   - `ackReceived()` 改成 only on matched ACK
   - `UP_TASK_LOG taskId mismatch` 直接返回 false，不再只警告
   - Linux 发送路径默认不再依赖 `waitForBytesWritten()`
   - Windows 发送路径已重新恢复更保守的 `waitForBytesWritten(200)`，避免基础握手丢失

## 3. 当前最重要的未完成/待确认项

### 3.1 当前仍未完全做完的验证

当前代码已经改了一轮，并且后续已补到这些结果：

1. Windows 本地 `jom` 编译通过  
2. RK3588 板端可正常启动  
3. 板端真机回传主链已通过  
4. 多任务门禁真机已通过  

当前仍未完全做完的主要只剩：

1. replay 回归实际执行  
2. 多帧 `UP_TASK_LOG` 在真实钥匙上的最终确认  
3. 不同业务样本（尤其送电任务）的进一步稳定性补样  

### 3.2 需要继续确认的高风险点

1. `KeySerialClient.cpp` 里的 ACK 处理仍需继续人工审查。
   - 当前已把 `ackReceived()` 改为 matched-only
   - 但 `SET_COM ACK` 的副作用目前保留为更宽松模式，用于避免 Windows 识别回归
   - 这里需要新 agent 再结合真机日志判断是否继续收紧
2. `Q_TASK` 直接响应门控这轮还没做满。
   - 当前只做了 P0 止血，不要误以为 generation 精修已经完成
3. 多帧 ACK `seq` 严格校验这轮还没上。
4. `ProcessService.cpp` 这轮没有动。

## 4. 当前更可信的执行策略

不要立刻继续扩状态机改动面，建议按这个顺序：

1. 先做编译验证
2. 先做 Windows 最小回归：
   - 是否还能稳定收到 `ACK for SET_COM`
   - 是否还能识别钥匙在位
   - 单帧传票是否还通
3. 再上开发板重点验证：
   - UI 是否明显更顺
   - 不进入工作台页时是否不立即拖慢
   - 回传是否仍卡在 `I_TASK_LOG -> ACK -> UP_TASK_LOG`
   - HTTP 成功后是否 `return-delete-pending -> return-delete-success`
4. 若板端仍失败，再继续：
   - 串口状态机 P1：更细 generation/token、direct response gating、多帧 ACK seq

## 4.1 2026-03-13 晚补充结论

已新增确认：

1. RK3588 板端在默认保守图形模式下可正常启动；此前的 `xcb_egl + GLES` 强制策略会触发：
   - `Cannot find EGLConfig`
   - `Unable to find an X11 visual which matches EGL config 0`
2. 当前 `src/app/main.cpp` 已改为：
   - 默认 `RK3568_LINUX_GL_MODE=auto`
   - 不再默认强制 `QT_XCB_GL_INTEGRATION=xcb_egl`
   - 仅在显式设置 `RK3568_LINUX_GL_MODE=egl/gles/xcb_egl_gles` 时才走激进图形路径
3. 真机已再次确认完整板端主链可收口：
   - `传票 -> KEY_REMOVED -> KEY_PLACED -> SET_COM -> Q_TASK(status=0x02) -> SET_COM -> I_TASK_LOG -> ACK -> UP_TASK_LOG -> HTTP回传 -> DEL -> Q_TASK(count=0)`
4. 当前仍能看到个别 `SET_COM` 需要一次 retry 后才收到 ACK。
   - 这更像“差底座/板端时序仍紧”，不是“主链继续卡死”
   - 建议先把它归入“待观察问题”，暂不立刻扩大 `KeySerialClient` 改动面
5. 回传业务门禁已按最新口径收紧：
   - 自动回传与手工回传都不再允许“钥匙里一条完成就先回传一条”
   - 当前改为：钥匙中的相关任务必须全部完成后，才允许开始回传
   - 该改动落在 `KeyManageController` 业务编排层，不涉及协议层扩改
6. 2026-03-16 真机已进一步确认：
   - “两条任务一条完成一条未完成”时，不会提前回传，手工回传按钮为灰色
   - “两条任务都完成”时，可自动逐条回传、逐条 `DEL`，最终回到 `Q_TASK(count=0)`
7. 当前离线补充内容已增加：
   - `test/replay/scenario_return_gate_incomplete_task_blocks_return.jsonl`
   - `docs/operations/RETURN_GATE_COMPANY_CHECKLIST_2026-03-16.md`

## 4.2 当前收尾状态（2026-03-16）

当前更适合继续做的事情：

1. replay 回归：
   - 多帧 `UP_TASK_LOG`
   - timeout/retry
   - 多任务回传门禁
2. 文档归档和回公司专项清单整理

当前本机会话内的编译结论：

1. shell 环境里直接不可见 `cl/nmake/jom/MSBuild`
2. 但用户已在 Qt Creator 中使用 `jom` 完成一次成功编译
3. 当前可确认本地 Windows Debug 构建已重新链接通过

当前不建议继续做的事情：

1. 继续扩大 `KeySerialClient` 改动面
2. 提前做 P1/P2 状态机精修
3. 大规模线程化或结构重排

## 5. 建议新 agent 先读哪些文档

按顺序：

1. `docs/README.md`
2. `docs/operations/STATUS_SNAPSHOT.md`
3. `docs/operations/MIGRATION_EXECUTION_LOG.md`
4. `docs/architecture/KEY_FLOW_OVERVIEW.md`
5. `docs/operations/VALIDATION_PLAYBOOK.md`
6. `docs/serial/PROTOCOL_FRAMES_REFERENCE.md`
7. `docs/serial/RETURN_PROTOCOL_BASELINE.md`
8. 本文件 `docs/operations/AGENT_HANDOFF_2026-03-13.md`

## 6. 建议新 agent 优先查看的代码

1. `src/features/key/protocol/KeySerialClient.*`
2. `src/features/key/application/KeyManageController.*`
3. `src/features/key/application/TicketReturnHttpClient.*`
4. `src/features/key/application/KeySessionService.cpp`
5. `src/shared/contracts/IKeySessionService.h`
6. `src/features/key/ui/keymanagepage.*`
7. `src/features/workbench/ui/workbenchpage.*`
8. `src/features/log/ui/logpage.*`
9. `src/app/main.cpp`

## 7. 当前建议关注的日志关键词

1. `ACK for SET_COM`
2. `SET_COM ACK without KEY_EVT, schedule Q_TASK probe`
3. `Q_TASK probe confirmed key present`
4. `Ignore stale retry timeout`
5. `I_TASK_LOG resp ignored: no active task log request`
6. `UP_TASK_LOG taskId mismatch with pending request`
7. `UP_TASK_LOG timeout after ACK`
8. `return-delete-pending`
9. `return-delete-success`
10. `协议通讯降级`

## 8. 当前对下一位 agent 的最重要提醒

1. 不要重写整个协议栈。  
2. 不要把“回包头 FrameNo 一定回显请求 FrameNo”当真值。  
3. 不要一口气把串口状态机所有 P1/P2 改动全上。  
4. 当前最重要的是先验证：
   - 旧链没回归
   - 板端减负是否有效
   - 回传链是否从“完全卡死”变成“至少能释放/能继续收口”  
