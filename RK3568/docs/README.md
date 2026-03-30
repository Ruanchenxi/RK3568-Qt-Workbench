# RK3568 文档总入口

Status: Active
Owner: 项目维护者（单人开发）
Last Updated: 2026-03-30

本文件只承担两件事：

1. 明确当前主线真值文档在哪里。
2. 告诉新接手的人应该先读什么、哪些只是历史归档。

## 1. 真值边界

1. 当前项目唯一有效的文档真值目录是：
   - `D:\\Desktop\\QTtest\\RK3568\\RK3568\\docs`
2. 工作区根目录下若出现 `D:\\Desktop\\QTtest\\RK3568\\docs\\...` 的零散副本或空文件：
   - 视为历史残留/异常副本
   - 不继续在那一套路径上维护内容
3. `docs/archive/` 只保存阶段性接手材料、旧方案和专项清单：
   - 可用于追溯
   - 不作为当前主线真值

## 2. 当前主线先读什么

如果你是没有历史上下文的新接手者，按这个顺序读：

1. `operations/STATUS_SNAPSHOT.md`
   目的：先确认当前项目做到哪里、哪些链路已经落地。
2. `operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`
   目的：先确认 RK3588 板端真实运行方式、UI 适配边界和当前部署策略。
3. `architecture/KEY_FLOW_OVERVIEW.md`
   目的：理解钥匙链、工作台 JSON 输入链、传票链、回传链。
4. `architecture/KEYBOARD_MODULE_ARCHITECTURE.md`
   目的：理解当前自定义键盘模块的主线、边界和禁区。
5. `operations/VALIDATION_PLAYBOOK.md`
   目的：知道每次改动后怎么做最小验证。
6. `operations/CONFIG_REFERENCE.md`
   目的：查关键配置项、默认值和兼容口径。

若需要继续深入，再读：

1. `PROJECT_CONTEXT.md`
2. `architecture/CURRENT_FOLDER_LAYOUT.md`
3. `architecture/CODEMAP.md`
4. `architecture/DEPENDENCY_RULES.md`
5. `serial/SERIAL_PACKET_AGENT_GUIDE.md`
6. `serial/PROTOCOL_FRAMES_REFERENCE.md`
7. `replay/REPLAY_SPEC.md`
8. `COMMENTING_GUIDE.md`

## 2.1 按问题类型怎么读

如果你是带着具体问题来的，建议直接按下面顺序读：

1. 板端显示 / 登录页 / 系统设置页 / 服务日志页 / 底部状态栏：
   - `operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`
   - `operations/STATUS_SNAPSHOT.md`
   - `operations/VALIDATION_PLAYBOOK.md`
   - `operations/CONFIG_REFERENCE.md`
2. 服务启动 / `start-all` / 端口占用 / 服务日志来源：
   - `operations/CONFIG_REFERENCE.md`
   - `operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`
   - `operations/STATUS_SNAPSHOT.md`
3. 钥匙协议 / 传票 / 回传 / `Q_TASK / I_TASK_LOG / UP_TASK_LOG / DEL`：
   - `architecture/KEY_FLOW_OVERVIEW.md`
   - `operations/KEY_RETURN_STABILITY_STATUS_2026-03-30.md`
   - `serial/PROTOCOL_FRAMES_REFERENCE.md`
   - `serial/SERIAL_PACKET_AGENT_GUIDE.md`
4. 自定义键盘 / 输入框 / 页面避让：
   - `architecture/KEYBOARD_MODULE_ARCHITECTURE.md`
   - `operations/CONFIG_REFERENCE.md`
   - `operations/VALIDATION_PLAYBOOK.md`

## 2.2 当前最常用的真值文档

如果只想看“最可能需要同步维护”的文档，优先关注这 4 份：

1. `operations/STATUS_SNAPSHOT.md`
2. `operations/CONFIG_REFERENCE.md`
3. `operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`
4. `PROJECT_CONTEXT.md`

## 3. 文档目录职责

1. `docs/architecture/`
   - 当前主线架构、结构边界、目录规则、代码地图。
2. `docs/operations/`
   - 当前状态、配置参考、验证清单、板端部署/UI 定稿策略。
3. `docs/serial/`
   - 串口协议、报文、链路基线和专项参考。
4. `docs/replay/`
   - 回放场景和无硬件验证规则。
5. `docs/archive/`
   - 历史 handoff、旧方案、迁移过程材料。
   - 仅用于追溯，不用于判断当前主线。

## 4. 当前工程约束

1. 运行目标：支持 Windows 开发验证与 RK3588 Ubuntu 部署。
2. UI 约束：`.ui` 文件固定在项目根 `ui/`。
3. 构建体系：继续使用 `qmake + .pri`。
4. 协议红线：`7E6C / CRC / 重试 / 稳定窗` 语义不得随意改动。
5. 串口红线：
   - `/dev/ttyS4` 仅钥匙链路
   - `/dev/ttyS3` 仅刷卡链路
6. 文档维护红线：
   - 若代码行为已改，而文档仍记录旧口径，必须同步修正文档真值
   - 优先更新 `operations/` 下的真值文档，不把现行规则写进 `archive/`

## 5. 每次改动后的最小验证

在项目根目录 `RK3568/` 下执行：

```powershell
.\tools\arch_guard.ps1 -Phase 3
```

至少完成以下确认：

1. 门禁输出 `No violations.`
2. 工程可编译通过（Debug 至少一次）
3. 主路径手工验证可通过：
   - 登录
   - 工作台
   - 钥匙管理
   - 系统设置
   - 服务日志

若改动涉及串口协议或会话流程，额外执行：

1. 真机链路验证（可用时）
2. replay 场景验证（半包/粘包/超时至少 1 个）

## 6. 如何处理旧文档

1. 新增主线规则或修改现行规则：
   - 更新当前真值文档
   - 不要把新内容继续写进 `docs/archive/`
2. 阶段性交接、专项清单、旧方案：
   - 进入 `docs/archive/`
3. 若改动了规则文档，需同步检查：
   - `tools/arch_guard.ps1`

## 7. 当前入口结论

如果你现在只想知道“先看哪一份”，答案是：

1. 改代码前先看 `operations/STATUS_SNAPSHOT.md`
2. 涉及板端 / UI / 服务日志时再看 `operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`
3. 涉及配置或服务启动时看 `operations/CONFIG_REFERENCE.md`
4. 改完后按 `operations/VALIDATION_PLAYBOOK.md` 做最小验证
