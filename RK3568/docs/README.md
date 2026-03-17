# RK3568 文档总入口

Status: Active  
Owner: 项目维护者（单人开发）  
Last Updated: 2026-03-16

本文件是 `docs/` 的总入口，目标是让“零历史新 Agent / 新同事”在 5 分钟内回答：

1. 这个项目是做什么的。  
2. 代码应该写在哪里、不能写在哪里。  
3. 改动后怎么验证并保证不回退。  

补充约定：

1. 当前项目唯一有效的文档真值目录是：
   - `D:\\Desktop\\QTtest\\RK3568\\RK3568\\docs`
2. 工作区根目录下若出现 `D:\\Desktop\\QTtest\\RK3568\\docs\\...` 的零散副本或空文件：
   - 视为历史残留/异常副本
   - 不继续在那一套路径上维护内容

## 1. 项目概览（10 行）

1. 项目定位：原产品闭环复刻 + 架构重构并行推进。  
2. 运行目标：支持 Windows 开发验证与 RK3588 Ubuntu 部署。  
3. 核心页面：工作台、钥匙管理、系统设置、服务日志、虚拟键盘、登录页。  
4. 当前状态：Feature-First Lite 目录迁移已完成 4 批。  
5. 开发策略：小步迭代，每步必须可编译、可运行、可回滚。  
6. 验证策略：真机优先，replay 为无硬件补充验证。  
7. UI 约束：`.ui` 文件固定在项目根 `ui/`，本阶段不迁移。  
8. 协议约束：7E6C/CRC/重试/稳定窗语义不得随意改动。  
9. 串口红线：`/dev/ttyS4` 仅钥匙链路，`/dev/ttyS3` 仅刷卡链路。  
10. 共享边界：两条链路只共享 `platform/serial` 抽象，不共享协议实现类。  

## 2. 新 Agent 推荐阅读顺序（严格按顺序）

### 2.1 如果你是“没有任何历史上下文的新 Agent”

先读：

1. `operations/NEXT_AGENT_BRIEF.md`
2. `operations/STATUS_SNAPSHOT.md`
3. `operations/MIGRATION_EXECUTION_LOG.md`

这 3 份是“现在做到哪、下一步干什么”的最快入口。

1. `PROJECT_CONTEXT.md`  
目的：先理解项目业务目标、平台边界、当前优先级。  

2. `architecture/CURRENT_FOLDER_LAYOUT.md`  
目的：确认代码当前真实落地位置（不是目标草案）。  

3. `architecture/CODEMAP.md`  
目的：快速定位“改某个需求应该动哪些文件”。  

4. `serial/SERIAL_PACKET_AGENT_GUIDE.md`  
目的：零历史情况下快速上手串口报文链路与协议字段组成。  
配套：`serial/PROTOCOL_FRAMES_REFERENCE.md`（逐字节帧拆解 / CRC 向量 / 完整时序图 / 易错对照表，改协议前必读）。  

5. `architecture/DEPENDENCY_RULES.md`  
目的：明确依赖红线与门禁规则，避免产生结构回退。  

6. `architecture/TARGET_FOLDER_LAYOUT.md`  
目的：理解长期目标结构与冻结决策。  

7. `replay/REPLAY_SPEC.md`  
目的：掌握无硬件回放验证方式。  

8. `operations/STATUS_SNAPSHOT.md`  
目的：先知道当前做到哪里、哪些链路已经落地。  

9. `operations/MIGRATION_EXECUTION_LOG.md`
目的：理解当前迁移进度、已拍板规则、下一阶段重点。  

10. `operations/VALIDATION_PLAYBOOK.md`  
目的：知道每次改动后该验证哪些功能。  

11. `COMMENTING_GUIDE.md`  
目的：保证新增代码注释质量一致。  

## 3. 当前工程状态（2026-03-11）

1. 迁移状态：Feature-First Lite 目录迁移已完成，传票主链已并入主程序。  
2. 传票状态：`工作台 JSON -> 本地 HTTP 接收 -> 系统票入池 -> 手动/自动传票` 主链已落地。  
3. 回传状态：单帧回传主链已落地并通过本地程序真机零回归验证。  
4. 初始化 / RFID：手工 `INIT(0x02)` 与手工 `DN_RFID(0x1A)` 已接入主程序，并通过真机 ACK 验证。  
5. 当前重点：继续收送电样本、真实多帧回传真机样本，以及后续业务收口。  
6. 构建体系：继续使用 `qmake + .pri`，暂不迁移 CMake。  
7. 架构门禁：使用 `tools/arch_guard.ps1 -Phase 3`。  
8. 串口体系：真机 `QtSerialTransport` 与回放 `ReplaySerialTransport` 可切换。  

## 4. 每次改动后的最小验证清单

在项目根目录 `RK3568/` 下执行：

```powershell
.\tools\arch_guard.ps1 -Phase 3
```

至少完成以下确认：

1. 门禁输出 `No violations.`。  
2. 工程可编译通过（Debug 至少一次）。  
3. 主路径手工验证可通过：登录、钥匙管理、系统设置、服务日志、工作台。  

若改动涉及串口协议或会话流程，额外执行：

1. 真机链路验证（可用时）。  
2. replay 场景验证（半包/粘包/超时至少 1 个）。  

## 5. 文档分类与用途

1. `docs/architecture/`：架构结构、依赖红线、迁移记录、代码地图。  
2. `docs/serial/`：串口报文专用开发手册（协议组成、链路改动与验收）。  
   - `SERIAL_PACKET_AGENT_GUIDE.md`：链路架构、状态机、改动流程、常见误区。  
   - `PROTOCOL_FRAMES_REFERENCE.md`：逐字节帧格式、各命令完整拆解、CRC 向量、拆包伪代码、常量速查。  
   - `TICKET_PROTOCOL_GUIDE.md`：传票 JSON / payload / frame(s) / ACK 续发专用手册。  
   - `RETURN_PROTOCOL_BASELINE.md`：回传链路当前实测基线（Q_TASK / I_TASK_LOG / UP_TASK_LOG / HTTP 回传）。
3. `docs/replay/`：回放规范、脚本格式、无硬件验证流程。  
4. `docs/operations/`：当前状态、配置参考、验证清单。  
   - `STATUS_SNAPSHOT.md`：当前项目做到哪里。  
   - `CONFIG_REFERENCE.md`：关键配置项与默认值。  
   - `VALIDATION_PLAYBOOK.md`：主路径 / 传票 / 真机 / replay 验证清单。  
   - `RETURN_GATE_COMPANY_CHECKLIST_2026-03-16.md`：回公司后验证“多任务全部完成门禁”和多任务自动回传收口的专项清单。  
   - `MIGRATION_EXECUTION_LOG.md`：当前移植进度、业务规则与下一步执行顺序。  
   - `NEXT_AGENT_BRIEF.md`：给无历史新 agent 的直接接手说明。  
   - `PRO_REVIEW_BRIEF_2026-03-12.md`：给 `pro` 做代码审查 / 业务逻辑 bug 排查 / RK3588 板端联调收口的集中简报。  
5. `docs/` 根目录：项目背景、注释规范、dead code 治理、总入口。  

## 6. 历史与治理文档说明

1. `architecture/MIGRATION_BATCH_PLAN.md`  
状态：Done（历史记录），用于回顾迁移过程与回滚定位。  

2. `DEAD_CODE.md`  
状态：治理文档，记录停用模块、原因与恢复步骤。  

3. `COMMENTING_GUIDE.md`  
状态：长期生效，所有新增/改动文件都需要遵循。  

## 7. 新增文档必须满足的规则

1. 放在正确分类目录（architecture/serial/replay/root）。  
2. 文档头至少包含 `Status`、`Owner`、`Last Updated`。  
3. 明确“适用范围”和“不适用范围”。  
4. 若是高频文档，必须在本文件中加入入口链接。  
5. 若改动了规则文档，需同步检查 `arch_guard.ps1` 是否也需要更新。  
