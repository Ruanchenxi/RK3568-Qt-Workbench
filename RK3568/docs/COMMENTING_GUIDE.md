# 注释规范（COMMENTING_GUIDE）

Status: Active  
Owner: 架构维护  
Last Updated: 2026-03-02  
适用范围：`src/**/*.h`、`src/**/*.cpp` 以及直接影响行为的脚本/配置代码。  

---

## 1. 目标与原则

本规范的目标不是“多写注释”，而是“让后续维护者在最短时间理解风险与边界”。

核心原则：

1. 注释优先解释 **WHY**（为什么这样做）。  
2. 代码本身表达 **WHAT**（做了什么），注释不要重复代码字面含义。  
3. 涉及协议、状态机、重试、硬件兼容的逻辑必须标记风险。  
4. 注释必须与当前实现一致，改代码就要同步改注释。  

---

## 2. 哪些地方必须写注释

### 2.1 必写（不写视为不合格）

1. 新增 `.h/.cpp` 文件的文件头。  
2. 协议与报文处理关键路径（帧解析、CRC、重试、超时、重同步）。  
3. 失败回退逻辑（例如 replay 失败后回落真实串口）。  
4. workaround（临时兼容）与已知限制。  
5. 非直观的业务规则或跨模块约束。  

### 2.2 选写（有必要时）

1. 对外暴露接口的副作用说明。  
2. 复杂循环、状态迁移、数据映射。  
3. 兼容旧产品行为的特殊分支。  

### 2.3 不建议写

1. “把 x 赋值给 y”这类重复代码语义的注释。  
2. 一眼可读且无风险的简单 getter/setter 注释。  

---

## 3. 文件头模板（推荐统一）

每个核心文件建议使用统一结构，最少包含职责、层级、依赖边界、风险说明。

```cpp
/**
 * @file KeySessionService.cpp
 * @brief 钥匙会话服务默认实现（真机串口/回放串口注入点）
 *
 * Layer:
 *   features/key/application
 *
 * Responsibilities:
 *   1) 编排命令执行与会话状态输出
 *   2) 连接 KeySerialClient 与上层 controller
 *   3) 处理 replay 开关与失败回退
 *
 * Dependencies:
 *   - Allowed: IKeySessionService, KeySerialClient, ISerialTransport
 *   - Forbidden: UI page classes
 *
 * Constraints:
 *   - 协议语义由 protocol 层维护，此文件不直接改帧逻辑
 *   - replay 脚本加载失败必须回退真实串口
 */
```

---

## 4. 分层注释要求（按目录执行）

### 4.1 `features/*/ui`

1. 写清“这个页面只负责渲染与交互，不做 IO”。  
2. 写清关键信号/槽触发到哪个 controller。  
3. 如果有表格展示逻辑，注明数据来自哪个 application 层对象。  

### 4.2 `features/*/application`

1. 写清“编排角色”，不要让读者误以为是底层实现。  
2. 写清输入来源（UI）和输出去向（UI/Service）。  
3. 写清失败如何向上反馈（notice/timeout/error）。  

### 4.3 `features/*/domain`

1. 写清类型/接口的业务语义。  
2. 明确此层不做 IO。  
3. 对 ports 接口写清“谁实现、谁调用”。  

### 4.4 `features/*/infra` 与 `platform/*`

1. 写清对接对象（HTTP/串口/系统服务）。  
2. 写清可替换性（例如 mock/replay/real）。  
3. 写清与上层抽象的映射关系。  

### 4.5 `features/key/protocol`

1. 必须写明协议不变量（帧头、长度规则、CRC、重试窗口）。  
2. 必须写明“为什么这样处理”，避免后续误改。  
3. 修改此层必须同时补充回归验证说明。  

### 4.6 `shared/contracts`

1. 写清这个接口/DTO 是跨几个 feature 共享。  
2. 写清稳定性预期（临时/长期）。  

---

## 5. 函数注释模板（关键函数）

关键函数建议包含如下结构：

```cpp
/**
 * @brief 发送并等待 ACK（带重试）
 * @param cmd 业务命令码
 * @param payload 已编码负载
 * @return true: 收到 ACK; false: 超时/NAK
 *
 * Preconditions:
 *   - 串口已打开
 *   - 当前状态允许发送该命令
 *
 * Side Effects:
 *   - 更新内部重试计数
 *   - 产生日志事件（WARN/ERR）
 *
 * Failure Strategy:
 *   - 超时后最多重试 3 次
 *   - 超过阈值发出 timeout 事件并保持端口可恢复
 */
```

---

## 6. 行内注释规则（短注释）

仅在以下情况写行内注释：

1. 魔法数字来源说明。  
2. 协议字段偏移说明。  
3. 跨平台差异说明。  
4. 临时绕过（必须带标签与后续动作）。  

推荐标签：

1. `TODO(owner,date)`：明确未来要做。  
2. `FIXME(owner,date)`：已知缺陷待修。  
3. `HACK(owner,date)`：临时方案，需给退出条件。  
4. `SAFETY`：强调改动风险。  
5. `REPLAY`：与无硬件回放相关的关键点。  

---

## 7. 协议改动专项注释要求（必遵守）

当新增或修改报文命令时，必须在链路上补齐注释：

1. `ui`：触发动作与输入校验。  
2. `application`：命令映射关系与回传事件。  
3. `service`：超时/重试策略和失败反馈。  
4. `protocol`：帧字段、CRC、兼容约束。  

必须包含一句“不可随意修改”的语义说明，例如：

1. “此重试窗口与原产品行为对齐，改动会导致握手时序偏离。”  
2. “CRC16 算法与设备固件耦合，不能替换。”  

---

## 8. 日志与注释协同规范

关键路径注释应和日志语句配套，满足可诊断性：

1. 注释写清“失败路径存在且预期会发生”。  
2. 日志输出需能对应到注释描述的分支。  
3. 若注释写了回退行为，日志必须有对应提示。  

示例：

1. 注释写“脚本加载失败回退真实串口”。  
2. 日志必须输出 `Replay script load failed ... fallback to real serial transport.`。  

---

## 9. 严禁事项

1. 注释掉大段旧代码并长期保留。  
2. 写与实现不一致的“历史注释”。  
3. 用注释掩盖坏设计（函数过长、命名混乱、职责错位）。  
4. 在高风险区域（protocol/service）无任何风险说明。  

---

## 10. 提交前自检清单

1. 文件头是否包含职责、层级、依赖边界。  
2. 关键函数是否写了前置条件与失败策略。  
3. 协议关键点是否有不可变语义说明。  
4. 注释是否与当前代码一致。  
5. 改动是否同步了相关文档（如 replay、dependency rules）。  

---

## 11. 与其他文档关系

1. 目录和文件职责：`docs/architecture/CURRENT_FOLDER_LAYOUT.md`、`CODEMAP.md`。  
2. 依赖边界：`docs/architecture/DEPENDENCY_RULES.md`。  
3. 回放行为：`docs/replay/REPLAY_SPEC.md`。  
4. 归档与回滚：`docs/DEAD_CODE.md`。  
