# 项目背景与开发上下文（PROJECT_CONTEXT）

Status: Active  
Owner: 产品/架构共同维护  
Last Updated: 2026-04-03

## 1. 文档目的

本文件用于让“没有历史上下文的人”快速理解：

1. 这个项目要解决什么问题。  
2. 当前做到什么程度。  
3. 当前哪些能力已经真实落地。  
4. 后续开发优先级是什么。  
5. 哪些边界不能随意突破。  

## 2. 项目定义（一句话）

RK3568 项目是对既有可运行产品进行功能复刻与工程重构的项目，目标在 PC 与 RK3588 Ubuntu 双端运行，并通过闭环验证逐步收敛到稳定版本。

## 3. 业务目标与工程目标

### 3.1 业务目标

1. 复刻原产品关键流程，确保用户侧行为一致。  
2. 支持钥匙管理、日志查看、系统配置、工作台业务。  
3. 支持多种登录模式（账号密码、刷卡、后续指纹）。  

### 3.2 工程目标

1. 将平铺代码收敛到可维护目录结构。  
2. 降低单人维护成本，提升定位速度与回滚安全性。  
3. 建立可执行门禁与无硬件验证路径。  
4. 形成对你自己和未来新 agent 都可用的低冗余文档体系。  

## 4. 运行平台与部署场景

1. 开发验证平台：Windows（Qt Creator + qmake）。  
2. 目标部署平台：RK3588 Ubuntu。  
3. 构建方式：当前维持 `qmake + .pri`，不在本阶段切 CMake。  

## 5. 当前项目主线（2026-04-03）

### 5.1 工作台 JSON / 传票链

当前已经形成：

- 工作台 JSON -> 本地 HTTP 接收
- `TicketStore` 入池
- 手动/自动传票
- 传票前补 `SET_COM -> ACK -> TICKET`
- 传票执行态持久化到 `logs/ticket_store.json`

### 5.2 回传链

当前已经形成：

- `Q_TASK(status=0x02)` 作为完成门禁
- 手工与自动都按任务单号独立回传
- 自动回传严格串行
- 回传前补 `SET_COM`
- `Q_TASK -> I_TASK_LOG -> UP_TASK_LOG -> HTTP -> DEL -> Q_TASK verify`
- 上传失败进入 `manual-required`

### 5.3 撤销链（cancelTicket）

当前已经形成：

- `/ywticket/WebApi/cancelTicket`
- `cancel-accepted / cancel-pending / cancel-executing / cancel-done`
- `cancel_ticket_journal.json` 最小撤销记录
- 撤销会冻结自动传票 / 自动回传 / 自动恢复
- `Q_TASK -> DEL -> Q_TASK verify` 的删除验证链
- 本地票不存在时支持 orphan recovery

### 5.4 持久化与恢复

当前已经形成：

- `TicketStore` 执行态落盘到 `logs/ticket_store.json`
- 程序启动时在 `KeyManageController::start()` 中加载
- 已完成票按 7 天策略清理
- orphan recovery 同时支持：
  - `cancelTicket` 被动恢复
  - `TasksUpdated` 主动恢复

### 5.5 系统设置 / 登录 / 服务日志 / 键盘

这些能力仍然存在，但对当前你和未来 agent 来说，它们不是进入项目的第一主线。
如果要理解项目，优先先掌握：

- key flow
- trans/cancel/return
- persistence / recovery
- validation / config

再回来看这些专题能力。

## 6. 当前真正优先的维护重点

1. 保持 trans/cancel/return 主链稳定。  
2. 继续补专项真机样本：
   - 持久化重启恢复
   - orphan recovery
   - 自动独立回传
   - `cancel-pending -> cancel-executing -> cancel-done`
3. 清理历史文档口径，让后续维护者只需读少量主文档。  
4. 多钥匙场景目前仅做数据预埋，不在当前阶段直接扩实现。  

## 7. 当前技术约束（必须遵守）

1. `.ui` 文件固定在项目根 `ui/`，不迁移。  
2. 协议关键语义（7E6C/CRC/重试/稳定窗）不随意改。  
3. 页面层不得直连底层实现（串口/网络/进程）。  
4. 每步改动必须满足可编译、可运行、可回滚。  
5. 结构调整后必须执行门禁：`./tools/arch_guard.ps1 -Phase 3`。  

## 8. 当前开发方法论

1. 小步改动：每次改动聚焦单一问题。  
2. 先让主链稳定，再做结构和体验。  
3. 代码真值优先，文档必须及时收口。  
4. dated brief / handoff / checklist 默认归档，不应继续当主文档。  

## 9. 新 Agent 最小上手路径（执行版）

1. 读 `docs/README.md`（总入口）。  
2. 读本文件（业务与约束背景）。  
3. 读 `operations/STATUS_SNAPSHOT.md`（当前状态）。  
4. 读 `architecture/KEY_FLOW_OVERVIEW.md`（关键主链）。  
5. 读 `operations/VALIDATION_PLAYBOOK.md`（验证方式）。  
6. 读 `operations/CONFIG_REFERENCE.md`（关键配置）。  
7. 若涉及串口协议改动，再读：  
   - `serial/SERIAL_PACKET_AGENT_GUIDE.md`  
   - `serial/PROTOCOL_FRAMES_REFERENCE.md`  
8. 修改后执行 `./tools/arch_guard.ps1 -Phase 3` 并完成主路径验证。  
