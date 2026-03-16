# 回公司专项验证清单（2026-03-16）

Status: Active
Owner: Codex
Last Updated: 2026-03-16
适用范围：回公司后对“多任务全部完成门禁 + 多任务自动回传收口 + 板端稳定性”做真机专项验证。
不适用范围：不替代协议逐字节文档，不替代常规 UI 冒烟。

## 1. 本清单目标

重点验证：

1. “两条任务一条完成一条未完成”时，自动回传和手工回传都被总门禁拦住。
2. “两条任务都完成”时，自动回传可以正常开始，并最终逐条清理完成。
3. 板端在默认保守图形模式下继续稳定启动。
4. 主链仍保持：
   - `Q_TASK(status=0x02) -> I_TASK_LOG -> ACK -> UP_TASK_LOG -> HTTP -> DEL -> Q_TASK(count=0)`

## 2. 启动命令

```bash
pkill -f /home/linaro/dev/build-rk3568/RK3568 || true
cd /home/linaro/dev/build-rk3568
unset QT_XCB_GL_INTEGRATION
unset QT_OPENGL
unset RK3568_LINUX_GL_MODE
DISPLAY=:0 ./RK3568 2>&1 | tee run.log
```

## 3. 场景 A：两条任务，一条完成，一条未完成

步骤：

1. 让钥匙中同时存在两条相关任务。
2. 仅完成其中一条，另一条保持未完成。
3. 放回钥匙。

预期：

1. 会先读取 `Q_TASK`。
2. 不会进入 `I_TASK_LOG`。
3. 不会发起 HTTP 回传。
4. 不会执行 `DEL`。
5. 手工 `回传(重试)` 按钮为灰色，或点击后仍被同一规则拦住。
6. 页面提示“必须全部任务完成后才允许回传”同等语义。

## 4. 场景 B：两条任务都完成

步骤：

1. 在场景 A 的基础上，把剩余那条也完成。
2. 再次放回钥匙。

预期：

1. 自动回传开始。
2. 先处理一条，再通过后续 `Q_TASK` 继续处理下一条。
3. 每次删除后都能再次看到 `Q_TASK resp: count=...`。
4. 最终回到 `Q_TASK resp: count=0`。
5. 页面最终进入 `return-delete-success`。

## 5. 现场重点记录

每轮至少记录：

1. 场景名称
2. `Q_TASK resp: count=...`
3. 是否出现 `I_TASK_LOG send`
4. 是否出现 HTTP 回传
5. 是否出现 `DEL`
6. 页面最终状态
7. 是否出现 `UP_TASK_LOG timeout after ACK`
8. 是否出现 `return-delete-pending` 卡住
