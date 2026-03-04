# DEAD_CODE 清单与治理策略

Status: Active (Governance/Archive)  
Owner: 架构维护  
Last Updated: 2026-03-02  
适用范围：`src/_archive/` 归档代码、停用模块治理、回滚恢复流程。  

---

## 1. 文档目标

本文件回答 4 个问题：

1. 什么代码可以判定为 dead/inactive。  
2. 当前哪些模块已经归档，为什么归档。  
3. 如何安全归档，避免误删与结构回退。  
4. 需要时如何恢复并验证。  

---

## 2. 术语定义

### 2.1 Dead Code（停用代码）

满足以下至少两条：

1. 主链路无调用路径。  
2. 与当前实现重复且保留会造成冲突。  
3. 不再符合当前架构分层。  
4. 保留在主编译链路会增加误用风险。  

### 2.2 Inactive Path（暂不启用路径）

代码结构保留但功能未上线，例如 auth 的刷卡/指纹 source 占位文件。  
这类代码不是 dead code，不进入 `_archive`。  

### 2.3 Archive（归档）

从主编译链路移除，保留源码用于追溯、对比、必要时恢复。  

---

## 3. 当前归档清单（已落地）

### 3.1 `services/AuthService.*`（历史实现）

1. 当前位置：`src/_archive/services/`。  
2. 归档原因：  
   - 与 `src/features/auth/infra/password/authservice.*` 职责重复。  
   - 同名类/头文件容易造成 include 冲突。  
   - 主链路已不依赖。  

### 3.2 `services/SerialService.*`（历史串口实现）

1. 当前位置：`src/_archive/services/`。  
2. 归档原因：  
   - 现行串口链路已统一为 `features/key + platform/serial`。  
   - 继续保留在主编译会形成双链路并存。  

### 3.3 `protocol/KeyCabinetProtocol.*`（旧协议模型）

1. 当前位置：`src/_archive/protocol/`。  
2. 归档原因：  
   - 与当前 7E6C 协议链路模型不一致。  
   - 在主工程无有效调用链。  

### 3.4 `protocol/ProtocolBase.h`（旧协议抽象）

1. 当前位置：`src/_archive/protocol/`。  
2. 归档原因：  
   - 仅服务于已归档的 `KeyCabinetProtocol`。  
   - 当前主链路无需该抽象。  

---

## 4. 归档治理总原则

1. 停编译优先于物理删除。  
2. 归档优先于“注释掉大段代码”。  
3. 归档后主链路禁止 include `_archive`。  
4. 归档操作必须可回滚。  
5. 归档动作必须更新文档（本文件 + CURRENT/CODEMAP 如有影响）。  

---

## 5. 新增 dead code 的标准流程

### 5.1 判定阶段

1. 用调用链确认主链路无依赖。  
2. 评估是否为“暂不启用”而非“应归档”。  
3. 评估是否与当前实现重复或冲突。  

### 5.2 归档阶段

1. 将目标文件移动到 `src/_archive/<domain>/`。  
2. 从 `RK3568.pro` 或相关 `.pri` 中移除编译项。  
3. 清理主链路 include。  
4. 在 `_archive/README.md` 或本文件补充原因。  

### 5.3 验证阶段

1. 全量编译通过。  
2. `.\tools\arch_guard.ps1 -Phase 3` 通过。  
3. 关键主链路手工验证通过。  

---

## 6. 恢复（回滚）流程

当归档模块必须恢复时，禁止直接回到旧平铺目录，必须恢复到当前架构目录。

### 6.1 恢复路径映射

1. 认证相关旧实现 -> `src/features/auth/infra/password/`。  
2. 串口实现 -> `src/platform/serial/`。  
3. 钥匙协议实现 -> `src/features/key/protocol/`。  

### 6.2 恢复步骤

1. 从 `_archive` 恢复目标文件到目标目录。  
2. 更新 include 路径。  
3. 更新 `RK3568.pro` / 对应 `.pri` 编译清单。  
4. 重新编译。  
5. 跑 `arch_guard`。  
6. 运行最小回归用例。  

### 6.3 最小回归用例

1. 应用启动成功。  
2. 登录链路可用。  
3. 钥匙链路最小命令可用（SET_COM / Q_TASK / DEL）。  
4. 日志页与系统设置页无明显回归。  

---

## 7. 禁止行为

1. 从 `_archive` 直接 include 到主链路。  
2. 恢复代码但不更新编译清单。  
3. 将旧平铺目录作为恢复目标（`src/services`、`src/protocol` 等）。  
4. 未验证直接删除归档源码。  

---

## 8. 建议的归档记录模板

新增归档项时，建议追加以下信息：

1. 模块路径。  
2. 归档日期。  
3. 归档原因。  
4. 替代实现路径。  
5. 恢复条件。  
6. 最小验证用例。  

---

## 9. 与其他文档关系

1. 当前目录结构：`docs/architecture/CURRENT_FOLDER_LAYOUT.md`。  
2. 文件职责索引：`docs/architecture/CODEMAP.md`。  
3. 依赖红线：`docs/architecture/DEPENDENCY_RULES.md`。  
4. 迁移历史：`docs/architecture/MIGRATION_BATCH_PLAN.md`。  
