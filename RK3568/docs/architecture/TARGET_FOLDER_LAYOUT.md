# 目标目录结构（单人开发可维护版）

Status: Active  
Owner: 架构维护  
Last Updated: 2026-03-02  
适用范围：定义本项目未来 6~12 个月的目标目录形态与分层职责。  

## 1. 设计目标

本目标结构不是“学术最优”，而是服务当前实际需求：

1. 单人开发时，能快速定位“代码应该放哪里”。  
2. 新功能接入时，变更影响范围可控（避免全局污染）。  
3. 业务层、协议层、平台层边界清晰，降低回归风险。  
4. PC 与 RK3588 双平台运行路径一致且可替换。  

## 2. 冻结决策（本阶段硬约束）

1. `.ui` 文件保持在项目根 `ui/` 目录，不迁移。  
2. 构建系统继续使用 `qmake`，暂不切换 CMake。  
3. 协议核心语义（7E6C/CRC/重试/稳定窗）不做结构重构式修改。  
4. 每次目录/依赖调整后必须可编译、可运行、可回滚。  

## 3. 目标目录树

```text
RK3568/
├─ src/
│  ├─ app/                                # 入口与应用装配
│  │  ├─ main.cpp
│  │  ├─ mainwindow.h/.cpp
│  │  └─ MainWindowController.h/.cpp
│  │
│  ├─ core/                               # 轻量通用核心（严格收敛）
│  │  ├─ ConfigManager.h/.cpp
│  │  └─ AppContext.h/.cpp
│  │
│  ├─ features/                           # 按业务域分包（核心）
│  │  ├─ auth/
│  │  │  ├─ ui/
│  │  │  ├─ application/
│  │  │  ├─ domain/
│  │  │  └─ infra/
│  │  │
│  │  ├─ workbench/
│  │  │  ├─ ui/
│  │  │  └─ application/
│  │  │
│  │  ├─ key/
│  │  │  ├─ ui/
│  │  │  ├─ application/
│  │  │  └─ protocol/
│  │  │
│  │  ├─ system/
│  │  │  ├─ ui/
│  │  │  └─ application/
│  │  │
│  │  └─ log/
│  │     ├─ ui/
│  │     └─ application/
│  │
│  ├─ platform/                           # 基础设施实现（可替换）
│  │  ├─ serial/
│  │  ├─ process/
│  │  └─ logging/
│  │
│  └─ shared/                             # 跨 feature 共享契约
│     └─ contracts/
│
├─ ui/                                    # .ui 资源文件（冻结）
├─ resources/
├─ test/
│  └─ replay/
└─ docs/
```

## 4. 分层职责与依赖方向

### 4.1 app 层

1. 负责程序入口、顶层装配、跨页面协调。  
2. 不实现具体协议/设备细节。  

### 4.2 feature 层

1. `ui`：页面渲染、交互收集。  
2. `application`：流程编排、状态组织、调用抽象接口。  
3. `domain`：领域类型与抽象契约。  
4. `infra`：与外部系统/设备/API 对接的具体实现。  

### 4.3 key/protocol 层

1. 放钥匙协议语义与状态机实现。  
2. 仅依赖 `ISerialTransport` 抽象，不依赖具体串口实现类。  

### 4.4 platform 层

1. 放串口/进程/日志等系统能力实现。  
2. 为 feature/application 提供可替换实现。  

### 4.5 shared/contracts 层

1. 放跨 feature 的稳定接口与 DTO。  
2. 不接收“短期、单 feature 临时类型”堆积。  

## 5. 为什么这个结构适合当前项目

1. 业务分区清晰：看路径即可判断归属。  
2. 变更可控：新增刷卡/指纹时只扩 auth 域，不污染 key 域。  
3. 平台隔离：真机串口与回放串口切换成本低。  
4. 风险可控：协议语义在 key/protocol 集中，减少误改概率。  

## 6. 文件放置决策表（新增代码必看）

1. 含 `QWidget` 的页面类：`features/<feature>/ui`。  
2. 处理按钮事件与页面流程：`features/<feature>/application`。  
3. 抽象接口、业务类型：`features/<feature>/domain` 或 `shared/contracts`。  
4. HTTP/串口/设备接入实现：`features/<feature>/infra` 或 `platform/*`。  
5. 协议帧解析与状态机：`features/key/protocol`。  

## 7. 与当前落地状态的关系

1. 当前真实结构见 `CURRENT_FOLDER_LAYOUT.md`。  
2. 当前代码已基本落在该目标框架内。  
3. 目标文档仍保留，用于防止后续改动回到旧平铺模式。  

## 8. 迁移与治理关系

1. 迁移历史记录：`MIGRATION_BATCH_PLAN.md`。  
2. 依赖红线：`DEPENDENCY_RULES.md`。  
3. 停用代码治理：`DEAD_CODE.md`。  

## 9. 当前阶段明确不做

1. 不切 CMake。  
2. 不做大规模文件重命名。  
3. 不重写协议核心行为。  
4. 不将 `.ui` 搬入 feature 目录。  
