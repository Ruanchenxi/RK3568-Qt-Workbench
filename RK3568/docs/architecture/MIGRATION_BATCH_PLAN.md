# 目录迁移批次计划（4 批执行记录）

Status: Done (2026-03-02)  
Owner: 架构维护  
Last Updated: 2026-03-02  
说明：本文件是 `TARGET_FOLDER_LAYOUT.md` 的落地执行记录，保留用于追溯与回滚。

## 1. 执行原则（迁移期间生效）

1. 每批必须满足：可编译、可运行、可回滚。  
2. 每批只做结构与依赖收口，不顺手重写业务逻辑。  
3. `.ui` 不迁移，协议语义不改。  
4. 每批完成后必须执行 `.\tools\arch_guard.ps1 -Phase 3`。  

## 2. 总体完成状态

1. 批次 1：已完成（key 编排层收口）。  
2. 批次 2：已完成（key 协议层收口）。  
3. 批次 3：已完成（platform 层收口）。  
4. 批次 4：已完成（auth/system/log/workbench 收口）。  
5. 构建清单：已完成 `.pri` 化聚合。  
6. 认证骨架：`ICredentialSource + IAuthGateway + AuthFlowCoordinator` 已落地。  

## 3. 批次详情（历史记录）

### 批次 1：收口 key 编排层（低风险）

目标：

1. 将钥匙管理编排代码集中到 `features/key/application`。  

主要动作：

1. 迁移 `controllers/key/*` 到 `features/key/application/*`。  
2. 修正 include 路径与 `RK3568.pro` 源文件清单。  

验收结果：

1. 编译通过。  
2. 钥匙管理页可打开。  
3. 串口日志表展示与导出功能可用。  

回滚方案：

1. 回退本批 commit。  
2. 或把文件移回旧路径并恢复 `RK3568.pro`。  

### 批次 2：收口 key 协议层（中风险）

目标：

1. 将 key 协议核心集中到 `features/key/protocol`。  

主要动作：

1. 迁移 `KeySerialClient/KeyProtocolDefs/KeyCrc16/LogItem`。  
2. 保持协议语义不变，仅做路径与依赖收口。  

验收结果：

1. 编译通过。  
2. 基础命令链路（SET_COM/Q_TASK/DEL）可运行。  

回滚方案：

1. 回退本批 commit。  
2. 恢复 `RK3568.pro` 文件列表与 include。  

### 批次 3：收口 platform 层（中风险）

目标：

1. 明确底层实现归属 `platform`。  

主要动作：

1. 串口实现迁移到 `platform/serial`。  
2. 进程与日志服务迁移到 `platform/process` 与 `platform/logging`。  

验收结果：

1. 编译通过。  
2. 真机/回放串口切换可用。  
3. 服务日志页主流程不回退。  

回滚方案：

1. 回退本批 commit。  
2. 恢复 include 与构建清单。  

### 批次 4：feature 收口与一致性收尾（中风险）

目标：

1. 将 auth/system/log/workbench 统一归位到 feature 分层。  

主要动作：

1. 页面与 controller 按 feature 分层归位。  
2. auth 域补齐后续扩展插槽（device/http/domain ports）。  
3. 清理历史平铺 include，配合 arch_guard 收紧规则。  

验收结果：

1. 编译通过。  
2. 登录、系统设置、服务日志、工作台主路径可运行。  
3. 门禁脚本无违规。  

回滚方案：

1. 按 feature 粒度回退，不做整仓回退。  

## 4. 迁移后遗留事项（持续治理）

1. 继续防止 include 回退到旧平铺路径。  
2. 持续更新 `CODEMAP.md` 与 `CURRENT_FOLDER_LAYOUT.md`。  
3. 对 `_archive` 中代码保持“可追溯但不可依赖”状态。  

## 5. 未来若新增迁移批次（模板）

每新增一个批次，按下列结构记录：

1. 目标（要解决什么）  
2. 范围（动哪些目录/文件）  
3. 风险（低/中/高）  
4. 验收（编译/门禁/主路径）  
5. 回滚（最小回滚步骤）  

## 6. 通用执行模板（保留）

1. 创建目标目录。  
2. 移动 `.h/.cpp`（文件名不改）。  
3. 修改 include。  
4. 修改 `RK3568.pro` / `.pri`。  
5. 本地编译。  
6. 运行 `arch_guard`。  
7. 做主路径手工验证。  
8. 以单批次 commit 提交。  
