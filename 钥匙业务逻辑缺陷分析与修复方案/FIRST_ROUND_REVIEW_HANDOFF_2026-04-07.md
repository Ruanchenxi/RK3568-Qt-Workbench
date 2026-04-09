# 第一轮代码审查交接文档

> 日期：2026-04-07
> 目的：给下一个审查 agent 快速建立上下文，直接进入 findings-first 审查
> 范围：本轮只覆盖“第一轮修复”相关改动，不包含第二轮 `P4 / P7` 深改

---

## 1. 本轮目标

本轮实际目标是先收口第一轮高优先级问题，重点不是重构，而是止血与语义收紧：

- `P1` 撤销请求增加物理状态守卫
- `P2` 删除链在 session 不确定时更保守，避免误用新钥匙数据
- `P3` `DEL` 验证失败后增加自动重试
- `P9` 孤儿恢复降级，不再直接当作正常成功票
- `P10` 人工失败不再静默转 `auto-pending`
- `P11` HTTP 传票安全超时要真正释放活动上下文

`P5` 本轮没有新增大改，因为当前 `TicketReturnHttpClient` 已经有 `QTimer + abort()` 超时保护。

---

## 2. 本轮主要改动文件

本轮主要改动集中在以下文件：

- `src/shared/contracts/SystemTicketDto.h`
- `src/features/key/application/TicketStore.h`
- `src/features/key/application/TicketStore.cpp`
- `src/features/key/application/KeyManageController.h`
- `src/features/key/application/KeyManageController.cpp`
- `src/features/key/ui/keymanagepage.cpp`

注意：

- 当前工作区里这套 `src/features/...` 新结构在此仓库快照下是 `untracked` 状态
- 因此不要完全依赖 `git diff` 做审查
- 请直接按文件内容做只读审查

---

## 3. 已完成的业务变化

### 3.1 `P1` 撤销守卫

位置：

- `src/features/key/application/KeyManageController.cpp`
- cancel ingress handler

当前行为：

- 当任务已经 `transferState == success`
- 且当前钥匙不在位 / 当前无法确认钥匙列表
- 撤销会被拒绝

目标：

- 防止“任务已经下发到钥匙并被现场拿走”时，工作台仍然误撤销

---

### 3.2 `P3` 删除链自动重试

位置：

- `src/features/key/application/KeyManageController.cpp`
- `finalizePendingReturnDelete(...)`
- `retryPendingReturnDelete(...)`

当前行为：

- 回传上传成功后进入删除验证链
- 如果 `Q_TASK` 仍看到待删任务，会自动重发 `DEL`
- 最大重试 3 次
- 超过 3 次进入 `return-delete-failed`

目标：

- 防止板端出现“上传成功但删除没收口，只能一直等”的情况

---

### 3.3 `P2` session 不确定时更保守

位置：

- `src/features/key/application/KeyManageController.cpp`
- `finalizePendingReturnDelete(...)`

当前行为：

- 当删除链被钥匙移除打断后，`m_pendingDeleteSessionId == 0` 视为 `sessionUnknown`
- 如果当前钥匙没有再次上报该待删任务，则只保持 `return-delete-verifying` 并继续等待
- 不再因为“当前 `Q_TASK count=0`”直接把旧删除链收口成功

当前保守规则：

- 只有当前钥匙再次明确上报同一个待删 `taskId`
- 才允许继续走 `retryPendingReturnDelete(...)`

目标：

- 避免“钥匙 A 删除链被打断后，插入钥匙 B，然后用 B 的 `count=0` 误判 A 删除成功”

---

### 3.4 `P9` 孤儿恢复降级

位置：

- `src/features/key/application/TicketStore.cpp`
- `ingestOrphanTicket(...)`

当前行为：

- 孤儿恢复对象不再直接落为：
  - `transferState = success`
  - `returnState = idle`
- 而是改为：
  - `transferState = orphan-recovered`
  - `returnState = manual-required`

目标：

- 保留“程序重启后可恢复未知钥匙任务”的能力
- 但不让它直接参与自动回传 / 自动删除

---

### 3.5 `P10` 人工失败不自动补发

位置：

- `src/features/key/application/KeyManageController.cpp`
- `TicketTransferFailed` 分支
- `recoverAutoTransferWhenKeyEmpty(...)`
- `startDirectWorkbenchTransfer(...)`
- `tryStartTicketTransfer(...)`

新增概念：

- `SystemTicketDto.transferTriggerSource`
  - `manual`
  - `automatic`

当前行为：

- 自动触发失败，允许转 `auto-pending`
- 人工触发失败，不再自动转 `auto-pending`
- 人工失败后保持 `failed`，必须人工重试

目标：

- 防止“用户点了一次失败后，系统悄悄又自动补发成功”

---

### 3.6 `P11` HTTP 传票安全超时真释放

位置：

- `src/features/key/application/KeyManageController.cpp`
- `m_httpTransactionSafetyTimer`
- `clearHttpTransferTransaction(...)`

当前行为：

- 60 秒安全超时触发后：
  - 强制清理 HTTP 传票活动上下文
  - 同时把对应票打回 `failed`

目标：

- 防止互斥锁卡死，导致后续所有工作台传票都被永久挡住

---

### 3.7 回传业务规则：必须全部完成后才允许回传

位置：

- `src/features/key/application/KeyManageController.cpp`
- `hasBlockingIncompleteKeyTask(...)`
- `tryAutoReturnCompletedTicket(...)`
- `canStartTicketReturn(...)`

当前行为：

- 自动回传前先检查当前钥匙中的所有任务
- 手动回传前也做同样检查
- 只要同一把钥匙中仍有未完成任务，就不允许回传

目标：

- 满足最新业务口径：
  - 不允许“同一把钥匙里一个任务完成、另一个未完成时，先回传已完成那一个”

---

## 4. 已知未完全收口点

### 4.1 `keymanagepage.cpp` 旧 hint 分支未物理删除

位置：

- `src/features/key/ui/keymanagepage.cpp`

当前状态：

- 运行时实际已经走新规则文案
- 旧的一组回传提示分支已经用 `#if 0` 封住，不参与编译和执行
- 但源码里仍然保留着 legacy 分支，后续可物理删除

影响：

- 运行无影响
- 但源码阅读会有噪音

---

### 4.2 `safety timer` 位置仍有脏注释残片风险

位置：

- `src/features/key/application/KeyManageController.cpp`

当前状态：

- 有效的 `connect(...)` 逻辑已经补上
- 但这块近期被多次补丁覆盖过，建议审查时重点确认构造函数顶部这段最终源码是否干净、是否只保留一套有效连接逻辑

影响：

- 这是编译级风险点
- 请优先审

---

## 5. 本轮未完成项

本轮没有做：

- `P4` 新状态 `transfer-verifying` 与 `reconcile` 大改
- `P7` `KeySerialClient` 延迟命令队列化
- Windows 编译验证
- 板端真机验证
- replay 回归

---

## 6. 建议审查顺序

建议另一个 agent 按下面顺序审：

1. `KeyManageController.cpp` 构造函数顶部
   - `m_httpTransactionSafetyTimer`
   - 超时 `connect(...)`
   - 是否存在注释吞代码、重复连接、语法不完整

2. `finalizePendingReturnDelete(...)`
   - `sessionUnknown`
   - `stillExists`
   - `retryPendingReturnDelete(...)`
   - 是否还有“未知当前钥匙接管旧删除链”的路径

3. `TicketTransferFailed` 与 `recoverAutoTransferWhenKeyEmpty(...)`
   - `transferTriggerSource`
   - 人工失败 / 自动失败分流是否一致

4. `ingestOrphanTicket(...)`
   - `orphan-recovered`
   - `manual-required`
   - 是否仍有自动回传参与路径

5. `canStartTicketReturn(...)` / `tryAutoReturnCompletedTicket(...)`
   - “全部任务完成后才允许回传”的规则是否完整

6. `keymanagepage.cpp`
   - 运行时 hint 是否与控制器真实规则一致
   - `#if 0` legacy 分支是否需要继续清理

---

## 7. 期望审查输出

希望另一个 agent 给出的结果是：

- findings first
- 只报真实问题，不做泛泛总结
- 每条问题尽量带：
  - 文件
  - 行号
  - 风险描述
  - 是否会导致编译错误 / 业务误判 / UI 误导

建议输出格式：

1. `P0/P1` 编译或运行阻断问题
2. `P1/P2` 业务误判或状态误收口问题
3. `P2/P3` UI 与真实规则不一致问题
4. 剩余可后续处理的清理项

