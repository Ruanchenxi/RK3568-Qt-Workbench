# 第一轮审查清单

> 用途：给审查 agent 直接逐项勾

---

## A. 编译级检查

- [ ] `KeyManageController.cpp` 构造函数顶部 `m_httpTransactionSafetyTimer` 相关代码语法完整
- [ ] `connect(m_httpTransactionSafetyTimer, ...)` 只保留一套有效逻辑
- [ ] `retryPendingReturnDelete(...)` 的声明与实现一致
- [ ] `SystemTicketDto.transferTriggerSource` 的新增字段在读写持久化时前后一致
- [ ] `keymanagepage.cpp` 中 `#if 0 / #endif` 配对正确

---

## B. 业务规则检查

### B1. 撤销守卫

- [ ] `transferState == success` 且钥匙不在位时，撤销会被拒绝
- [ ] 钥匙在位但任务未实际执行完成时，不会误拒绝合理撤销

### B2. 删除链收口

- [ ] `DEL` 后 `Q_TASK` 仍能看到任务时，会进入自动重发 `DEL`
- [ ] 重试上限为 3 次
- [ ] 超过 3 次进入 `return-delete-failed`

### B3. session 不确定

- [ ] 删除链被钥匙移除打断后，不会因为当前钥匙 `count=0` 就直接判成功
- [ ] 只有当前钥匙再次明确报出同一个待删任务时，才允许继续 `DEL`

### B4. 人工 / 自动失败分流

- [ ] 自动触发失败仍允许转 `auto-pending`
- [ ] 人工触发失败不会自动转 `auto-pending`
- [ ] `recoverAutoTransferWhenKeyEmpty(...)` 不会回收人工失败任务

### B5. 孤儿恢复

- [ ] `ingestOrphanTicket(...)` 不再直接生成 `success/idle`
- [ ] 孤儿恢复对象不会直接参与自动回传

### B6. 回传前置规则

- [ ] 自动回传会检查当前钥匙中是否还有未完成任务
- [ ] 手动回传也会检查当前钥匙中是否还有未完成任务
- [ ] 规则符合“必须当前钥匙里的任务全部完成后才允许回传”

---

## C. UI 口径检查

- [ ] `keymanagepage.cpp` 运行时文案不再提示“单任务可独立回传”
- [ ] `return-delete-failed` 有明确提示
- [ ] `orphan-recovered` 有明确提示
- [ ] legacy 提示分支即使还在源码中，也不会参与运行

---

## D. 未验证项提醒

以下项目当前尚未验证，请不要误判为已闭环：

- [ ] Windows 编译
- [ ] Linux / 板端编译
- [ ] 板端真机业务回归
- [ ] replay 回归

