# 验证手册（VALIDATION PLAYBOOK）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-06  
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

## 2. 钥匙旧链路零回归

至少回归：

1. `初始化`
2. `获取任务数 (Q_TASK)`
3. `删除票 (DEL)`
4. `串口报文` 页日志追加
5. 专家模式 / Hex 显示 / CSV 导出

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

## 6. 回放验证

参考：`docs/replay/REPLAY_SPEC.md`

后续需要补充：

1. 传票单帧 replay
2. 传票多帧 replay
