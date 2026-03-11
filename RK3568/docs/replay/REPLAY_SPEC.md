# Replay 串口回放规范（REPLAY_SPEC）

Status: Active  
Owner: 架构维护  
Last Updated: 2026-03-10  
适用范围：`platform/serial/ReplaySerialTransport` 与 `KeySessionService` 回放注入流程。  

---

## 1. 文档目标

本规范用于无硬件阶段验证串口主链路，保证以下能力可重复验证：

1. 半包/粘包处理。  
2. 超时与重试行为。  
3. 错误分支（error event）与 UI 诊断反馈。  
4. 回放失败时回退真实串口行为。  

---

## 2. 当前实现对齐（代码事实）

当前回放链路行为（已在代码实现中）：

1. 开关键：`serial/replayEnabled`（默认 false）。  
2. 脚本键：`serial/replayScript`（默认 `test/replay/scenario_half_and_sticky.jsonl`）。  
3. 注入点：`features/key/application/KeySessionService.cpp`。  
4. 回退策略：脚本加载失败 -> 自动回退 `QtSerialTransport`。  
5. 回放执行器：`platform/serial/ReplaySerialTransport.cpp`。  

---

## 3. 脚本文件格式

### 3.1 编码与组织

1. UTF-8 文本文件。  
2. 每行一个 JSON 对象（JSONL）。  
3. 空行和 `#` 注释行被忽略。  

### 3.2 通用字段

每个动作对象可包含字段：

1. `op`：动作类型（必填）。  
2. `delayMs`：当前动作执行后，到下一动作的延迟毫秒（可选，默认 0）。  

注意：当前实现是“执行当前动作后，再按当前行的 `delayMs` 调度下一动作”。

---

## 4. 支持动作（当前实现）

### 4.1 `open`

用途：触发 opened 状态事件。  
示例：

```json
{"op":"open","delayMs":0}
```

### 4.2 `chunk`

用途：向读缓冲注入字节并触发 `readyRead`。  
字段：

1. `hex`：十六进制字节字符串，可带空格/换行。  

示例：

```json
{"op":"chunk","hex":"7E 6C FF 00 00 00 02 00 5A 0F A7 20","delayMs":20}
```

### 4.3 `timeout`

用途：仅表达时间流逝，不发数据。  
示例：

```json
{"op":"timeout","delayMs":1000}
```

### 4.4 `error`

用途：触发 `errorOccurred(code, text)`。  
字段：

1. `code`：整数错误码。  
2. `text`：错误文本。  

示例：

```json
{"op":"error","code":3,"text":"mock crc fail","delayMs":0}
```

---

## 5. Hex 解析规则（当前实现）

1. 解析前会移除空格、`\t`、`\r`、`\n`。  
2. 使用 `QByteArray::fromHex` 转换。  
3. `hex` 为空或解析结果为空时，不会触发 `readyRead`。  

---

## 6. 执行时序与边界

1. `open()` 被调用后，定时器立即触发第一条动作。  
2. 每次执行一条动作后，若仍有后续动作，按该条 `delayMs` 继续。  
3. 动作耗尽后停止调度。  
4. `close()` 会停止定时器并关闭回放状态。  

---

## 7. 启用方式（运行步骤）

在 `ConfigManager` 中设置：

1. `serial/replayEnabled=true`  
2. `serial/replayScript=test/replay/<your_scenario>.jsonl`  

启动后流程：

1. `KeySessionService` 优先尝试创建 `ReplaySerialTransport`。  
2. 脚本加载成功 -> 使用回放传输。  
3. 脚本加载失败 -> 记录 warning 并自动回退 `QtSerialTransport`。  

---

## 8. 建议场景集（最小验收）

### 8.1 半包场景

目标：验证分段接收重组能力。  
做法：同一帧拆成多条 `chunk`。  

### 8.2 粘包场景

目标：验证多帧连续数据切分能力。  
做法：多帧合并在同一 `chunk`。  

### 8.3 超时场景

目标：验证重试与超时日志分支。  
做法：插入 `timeout` 动作并控制间隔。  

### 8.4 错误事件场景

目标：验证错误事件链路与 UI 反馈。  
做法：插入 `error` 动作。  

---

## 9. 推荐脚本命名规范

建议采用：

1. `scenario_half_and_sticky.jsonl`  
2. `scenario_timeout_retry_3x.jsonl`  
3. `scenario_error_crc_like.jsonl`  
4. `scenario_mixed_long_run.jsonl`  
5. `scenario_up_task_log_multiframe.jsonl`

命名应体现“输入特征 + 预期行为”。

---

## 10. 限制与注意事项

1. 当前回放不校验“写入内容与回包动作”的因果关系。  
2. `write()` 仅返回写入长度，不驱动自动响应。  
3. 如需更精细校验（请求-响应匹配），需扩展回放协议。  

---

## 11. 调试建议

1. 脚本先从最小动作开始，逐步追加复杂场景。  
2. 每个复杂脚本先单独验证，再并入回归集合。  
3. 出现异常优先检查：  
   - 脚本路径是否正确  
   - JSON 是否合法  
   - `hex` 是否可解析  
   - `replayEnabled` 是否开启  

补充场景说明：

1. `scenario_up_task_log_multiframe.jsonl`
   - 目标：验证 `UP_TASK_LOG(0x15)` 两帧累计与逐帧 ACK
   - 注意：该场景主要验证协议层拼帧能力，不代表真实钥匙时序已被真机确认

---

## 12. 与其他文档关系

1. 依赖边界与门禁：`docs/architecture/DEPENDENCY_RULES.md`。  
2. 目录与模块定位：`docs/architecture/CURRENT_FOLDER_LAYOUT.md`。  
3. 文件职责索引：`docs/architecture/CODEMAP.md`。  
4. 注释要求：`docs/COMMENTING_GUIDE.md`。  
