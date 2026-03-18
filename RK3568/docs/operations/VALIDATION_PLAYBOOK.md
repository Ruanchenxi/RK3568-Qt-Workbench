# 验证手册（VALIDATION PLAYBOOK）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-16  
适用范围：主程序各链路验证步骤。  
不适用范围：不替代协议字段文档。  

## 1. 每次改动后的最低验证

1. 执行：
   ```powershell
   .\tools\arch_guard.ps1 -Phase 3
   ```
2. Qt Creator 中至少完成一次 Debug 编译。  
3. 主路径打开无崩溃：
   - 登录
   - 工作台
   - 钥匙管理
   - 系统设置
   - 服务日志
4. Linux 板端启动时检查一次图形诊断日志：
   - `QT_QPA_PLATFORM`
   - `QT_XCB_GL_INTEGRATION`
   - `QTWEBENGINE_CHROMIUM_FLAGS`
   - `AA_UseOpenGLES`
5. 若本轮改动已在 Qt Creator 中完成一次 `jom` 编译并成功链接，可将“Windows 本地编译通过”记为已完成；但 replay 仍需单独执行，不可用编译通过替代。
6. 若本轮涉及登录辅助链，额外确认：
   - 登录页 `选择账号` 按钮可见
   - 账号列表接口失败时，仍可手输账号登录

## 2. 钥匙旧链路零回归

至少回归：

1. `初始化`
2. `获取任务数 (Q_TASK)`
3. `删除票 (DEL)`
4. `串口报文` 页日志追加
5. 专家模式 / Hex 显示 / CSV 导出
6. HTTP 客户端 / 服务端报文“清空”后切页不会复活旧日志
7. 钥匙管理页顶部状态能区分：
   - 通讯: 断开
   - 通讯: 待钥匙
   - 通讯: 稳定中
   - 通讯: 待握手
   - 通讯: 待确认
   - 通讯: 恢复中
   - 通讯: 钥匙已就绪
   - 顶部只显示短状态词，详细原因仍在底部状态栏
9. 底部状态栏不应被 steady-state 消息覆盖：
   - `钥匙已稳定，等待通讯确认`
   - `钥匙已就绪，可发送 Q_TASK/DEL`
   - `自动传票已开启，但当前没有待发送系统票`
   这些信息应由顶部设备状态或页面静态提示承担，而不是长期占用底部状态栏
8. `当前配置座号` 显示当前配置站号，而不是固定 `--`

## 3. 工作台 JSON 输入链验证

检查点：

1. `HTTP服务端报文` 页能看到：
   - `[HTTP-INGRESS] listening ...`
   - `OPTIONS ... -> 200 OK`
   - `POST /ywticket/WebApi/transTicket`
2. `logs/http_ticket_*.json` 有新文件。  
3. `系统票数据` 表新增记录。  
4. 左侧“当前选中票据”摘要随选中项联动。  

## 4. 传票手动链验证

### 无硬件阶段

1. 选中系统票后点“传票”，不弹本地 JSON 文件选择框。  
2. 若串口未连接，应提示：
   - `串口未连接，无法发送传票`
3. 若串口已连接但钥匙未就绪，应提示：
   - `钥匙未就绪，无法发送传票`
4. 系统票状态不应莫名其妙变成 `success`。  

### 真机阶段

1. 传票单帧样本可发送。  
2. 传票多步单帧样本可发送。  
3. 传票多帧样本可续发。  
4. `串口报文` 页能看到发送、ACK 和失败原因。  

## 5. 自动传票验证

前提：`ticket/autoTransferEnabled=true`

1. 新 JSON 入池后，系统票状态从 `received` 到 `auto-pending` 或直接 `sending`。  
2. 自动传票不会绕过：
   - `connected`
   - `keyPresent`
   - `keyStable`
   - `sessionReady`
3. 自动传票失败时，状态应回写为 `failed`，并附带错误原因。  
4. 若底座抖动导致本地系统票仍停在 `sending/auto-pending/failed`，但随后 `Q_TASK` 已读到钥匙中存在该任务，则系统票应自动被纠正为 `success`。  
5. 若再次放下钥匙后，`Q_TASK` 成功确认钥匙中无任务，而系统票仍卡在 `sending/failed`，则应被回退为 `auto-pending` 并自动再次尝试传票。  

## 6. 删除钥匙票后的再次传票验证

1. 先让某条系统票成功传票到钥匙。  
2. 读取钥匙票列表，确认钥匙中存在该任务。  
3. 在 `钥匙票数据` 表中明确选中目标任务。  
4. 执行 `删除钥匙票 (DEL)`，并再次读取钥匙票列表确认任务已消失。  
5. 未选中钥匙票时点击删除，应提示：
   - `未选中钥匙票，无法删除，请先读取钥匙票列表并选中一条任务`
6. 此时对应系统票状态应变为：
   - `钥匙任务已删除，可再次传票`
7. 再次从工作台触发同一任务，或手动点击系统票“传票到钥匙”。  
8. 预期：
   - 允许重新进入传票发送链
   - 不再被“已传票到钥匙，本次不重复下发”错误拦住
## 7. 回放验证

参考：`docs/replay/REPLAY_SPEC.md`

至少验证：

1. 传票单帧 replay
2. 传票多帧 replay
3. `UP_TASK_LOG` 多帧 replay：
   - `I_TASK_LOG resp: totalFrames=2`
   - `UP_TASK_LOG frame 1/2 buffered`
   - `ACK for UP_TASK_LOG frame 0`
   - `UP_TASK_LOG frame 2/2 buffered`
   - `ACK for UP_TASK_LOG frame 1`
   - `UP_TASK_LOG parsed: ... frames=2`
4. 多任务回传门禁 replay：
   - `test/replay/scenario_return_gate_incomplete_task_blocks_return.jsonl`
   - 重点确认在“一条完成、一条未完成”时不会进入回传链

## 8. 回传自动触发验证

1. 真实钥匙完成任务后放回底座。  
2. 不手工点击“读取钥匙列表”。  
3. 预期：
   - 钥匙 ready 后主程序自动触发一次 `Q_TASK`
   - 若 `Q_TASK` 已确认钥匙中存在该任务，而本地系统票仍停在 `sending/auto-pending/failed`，则应先自动纠正为 `success`
   - 若 `status=0x02`，自动进入回传链
   - 若 `status=0x00/0x01`，提示 `任务未全部完成，暂不回传，请完成全部步骤后再放回钥匙`
   - 未完成场景下不回传、不 DEL
   - 若 HTTP 回传成功，但后续 `Q_TASK` 仍发现该已完成任务存在，则应进入 `return-delete-pending`，并自动继续 `DEL` 清理
   - 只有 `Q_TASK` 明确确认任务已不存在时，才进入 `return-delete-success`
4. 多任务场景新增门禁（2026-03-13 晚）：
   - 自动回传和手工回传都必须等钥匙中的相关任务**全部完成**后才允许开始
   - 若钥匙里仍有任意未完成任务，则当前应统一表现为：
     - 不进入 `I_TASK_LOG`
     - 不发起 HTTP 回传
     - 不执行 `DEL`
     - 手工 `回传` 按钮应禁用或提示不可回传
     - 页面提示“暂不回传，请全部任务完成后再放回钥匙”
   - 真机专项清单参考：
     - `docs/operations/RETURN_GATE_COMPANY_CHECKLIST_2026-03-16.md`

## 9. 初始化 / 下载 RFID 手工链验证

1. 钥匙 ready 后，点击“初始化”。  
2. 预期：
   - 先出现 HTTP `package-data`
   - 再出现 `INIT_MORE(0x82)` 与 `INIT(0x02)` 串口发送
   - 收到对应 ACK
3. 钥匙 ready 后，点击“下载 RFID”。  
4. 预期：
   - 先出现 HTTP `rfid-data`
   - 再出现 `DN_RFID(0x1A)` 串口发送
   - 收到 `ACK(5A 1A 00 00)`
5. 接入 `INIT / 下载 RFID` 后，再做一次“什么都不点”的自动回传零回归，确认不会自动插入这两条手工链。  

## 10. 板端减负验证

1. 启动后不进入工作台页时，不应立即创建 `QWebEngineView`。  
2. 串口日志高速追加时，`串口报文` 页应以合批方式刷新，不再逐条滚动到底部。  
3. 隐藏 `HTTP客户端/HTTP服务端` 或 `服务日志` 页时，不应继续高频重绘文本控件。  
4. 板端回传日志验证时，重点检查：
   - `Ignore stale retry timeout`
   - `I_TASK_LOG resp ignored: no active task log request`
   - `UP_TASK_LOG taskId mismatch with pending request`
   - `return-delete-pending`
   - `return-delete-success`

## 11. RK3588 板端联调判读补充（2026-03-13）

### 11.1 当前可接受现象（不应单独判故障）

1. 应用启动日志出现：
   - `RK3568_LINUX_GL_MODE= auto`
   - `AA_UseOpenGLES= false`
   这表示当前采用保守图形策略，不再默认强制 `xcb_egl + GLES`。
2. 启动后在**钥匙尚未放上**前出现：
   - `SET_COM ACK without KEY_EVT, schedule Q_TASK probe`
   - `Startup Q_TASK probe timeout, key remains not-present`
   这是当前 startup probe 的正常分支，不属于故障。
3. 在差底座场景下，回传前的补 `SET_COM` 偶发出现：
   - `Retry 1/3 for SET_COM`
   随后若仍能继续进入：
   - `I_TASK_LOG`
   - `UP_TASK_LOG`
   - `DEL`
   则当前应先视为“链路偏慢但可恢复”，不要单凭这一条就判失败。
4. 拔钥匙后出现：
   - `协议通讯降级: key removed`
   当前应视为设备态退回，不等于业务链异常。

### 11.2 当前应判为故障/待修的现象

1. 板端再次出现图形启动失败：
   - `Cannot find EGLConfig`
   - `Unable to find an X11 visual which matches EGL config`
2. 已收到 `I_TASK_LOG resp` 并已发送 `ACK(5A 05 00 00)` 后，又出现：
   - `UP_TASK_LOG timeout after ACK`
3. HTTP 已成功，但系统票长期停在：
   - `return-delete-pending`
   且后续没有进入：
   - `return-delete-success`
4. `DEL ACK` 已到，但后续 `Q_TASK` 仍长期读到相同 `status=0x02` 任务，且系统没有继续自动清理。
5. 已出现合法业务响应（如 `Q_TASK/I_TASK_LOG/UP_TASK_LOG/DEL`），却仍被旧 timeout 或旧回包重新打回不可用状态。

### 11.3 当前推荐的板端连续验证顺序

1. 连续做 3~5 轮：
   - 单步任务传票
   - 拔钥匙
   - 放回
   - 自动回传
   - `DEL`
   - `Q_TASK(count=0)`
2. 再做 1~2 轮多步任务，确认不只是单步样本稳定。
3. 再做送电任务，确认不是只在停电任务上收口。
4. 每轮重点记录：
   - 是否出现 `UP_TASK_LOG timeout after ACK`
   - 是否出现 `return-delete-pending` 卡住
   - `SET_COM` 是否经常需要 retry 后才 ACK
   - 顶部通讯状态是否与真实业务状态一致

## 12. 登录页账号选择验证

1. 进入账号密码登录页。  
2. 预期：
   - 用户名输入框右侧可见 `选择` 按钮
3. 点击 `选择`。  
4. 若接口可用，预期：
   - 可弹出账号列表选择框
   - 选中后会回填用户名输入框
5. 若接口失败，预期：
   - 页面提示获取失败
   - 仍允许手工输入用户名和密码，不应卡死登录流程
6. 若现场接口与 `system/apiUrl + /list-account` 不一致，可配置：
   - `auth/accountListUrl`

## 13. 虚拟键盘第一期验证（待功能落地后执行）

### 13.1 适用范围

仅适用于本地 Qt Widgets 页面：

1. 登录页
2. 系统设置页

不适用于：

1. 工作台 QWebEngine 页面

### 13.2 页面避让验证

1. 在登录页/系统设置页点击支持键盘的输入字段。  
2. 预期：
   - 底部虚拟键盘以非模态方式出现
   - 当前输入字段仍保持在键盘上方可见区域
   - 页面是“让位/滚动”，而不是整页缩放变形
3. 保存按钮、标题、字段标签不应被完全挡住。  

### 13.3 字段分型验证

1. 密码字段：
   - 应使用全键盘
2. 站号、端口等数字字段：
   - 应优先使用数字键盘
3. `homeUrl`、`apiUrl` 等地址字段：
   - 应使用 URL 键盘
   - 应显式提供 `: / . -` 等高频按键
4. 账号字段：
   - 优先通过 `选择账号` 完成，不以全键盘输入为主

### 13.4 工作台边界验证

1. 切到工作台页。  
2. 预期：
   - 应用内虚拟键盘不会主动接管工作台输入
   - 工作台页不会因为本地键盘机制而缩放或改布局

## 14. 自定义 QWidget 键盘第一阶段验证（当前进行中）

### 14.1 适用范围

当前第一阶段适用于：

1. 登录页
2. 系统设置页
3. `QLineEdit`
4. `QComboBox::lineEdit()` 的基础目标识别

当前不适用于：

1. 工作台页正式输入主线
2. 更复杂的 Web 输入桥接

### 14.2 统一触发验证

1. 先点击一个输入框。  
2. 再点击顶部 `虚拟键盘` 按钮。  
3. 预期：
   - 键盘弹出
   - 不因点击输入框自动弹出
   - 再次点击顶部 `虚拟键盘` 按钮可收起
4. 若当前输入目标是可编辑输入框，应不再弹出“请先点击一个 QLineEdit”类旧提示。

### 14.3 字段分型验证

1. 登录页 `usernameEdit/passwordEdit`：
   - 默认普通全键盘
2. 系统设置页 `homeUrlEdit/apiUrlEdit`：
   - 默认 URL 键盘
   - 应支持 `URL -> ABC -> 123 -> URL` 闭环切换
3. 系统设置页 `stationIdEdit/tenantCodeEdit`：
   - 默认数字键盘
   - 应支持 `123 -> ABC -> 123` 闭环切换
   - 不应出现无意义的 URL 路径

### 14.4 打开状态下切字段验证

1. 键盘保持打开。  
2. 在系统设置页从 URL 字段切到数字字段。  
3. 预期：
   - 键盘自动切到数字布局
   - 模式切换按钮集合同步刷新
4. 再从数字字段切回 URL 字段。  
5. 预期：
   - 键盘自动切回 URL 布局

### 14.5 页面避让验证

1. 登录页：
   - 不应整页下移或整体变形
   - 顶部导航、左侧品牌区、底部状态栏不应被键盘联动破坏
2. 系统设置页：
   - 仅允许 `formScrollArea` 局部滚动
   - 不应整页缩放
   - 当前输入框应尽量保持在键盘上方可见区

### 14.6 串口字段收口验证

1. 系统设置页 `钥匙串口 / 读卡串口`：
   - 只允许下拉选择
   - 不允许自由手输
2. `波特率 / 数据位`：
   - 应显示固定值
   - 不应允许编辑

### 14.7 中文输入最小闭环验证

1. 在普通全字符键盘场景打开键盘。  
2. 默认语言应为中文模式。  
3. 输入：
   - `ni`
   - `shi`
   - `zhong`
4. 预期：
   - 顶部固定候选栏出现候选词
   - 候选栏不应再动态撑高键盘
   - 第一排键帽不应与候选栏重叠
5. 点击候选词后，预期：
   - 候选词提交到当前输入框
6. 切回英文模式后，预期：
   - 字母直接进入输入框
   - 不再经过候选栏提交

### 14.8 当前应判为待修的现象

出现以下任一现象，应继续修正后再推进：

1. 登录页整体被向下挤压或版式跑偏
2. 键盘模式切换不是闭环
3. 键盘打开状态下切换输入框后，布局不更新
4. 系统设置页因键盘弹出导致整页缩放/主窗口变形
5. 中文候选出现后把键盘整体再次撑高
6. 中文候选栏与第一排键帽重叠
7. 点可编辑输入框后仍提示“请先点击一个 QLineEdit”
