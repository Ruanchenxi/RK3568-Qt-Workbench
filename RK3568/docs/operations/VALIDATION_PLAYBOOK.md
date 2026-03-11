# 验证手册（VALIDATION PLAYBOOK）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-11  
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
6. HTTP 客户端 / 服务端报文“清空”后切页不会复活旧日志

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

## 6. 删除钥匙票后的再次传票验证

1. 先让某条系统票成功传票到钥匙。  
2. 读取钥匙票列表，确认钥匙中存在该任务。  
3. 执行 `删除钥匙票 (DEL)`，并再次读取钥匙票列表确认任务已消失。  
4. 此时对应系统票状态应变为：
   - `钥匙任务已删除，可再次传票`
5. 再次从工作台触发同一任务，或手动点击系统票“传票到钥匙”。  
6. 预期：
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

## 8. 回传自动触发验证

1. 真实钥匙完成任务后放回底座。  
2. 不手工点击“读取钥匙列表”。  
3. 预期：
   - 钥匙 ready 后主程序自动触发一次 `Q_TASK`
   - 若 `status=0x02`，自动进入回传链
   - 若 `status=0x00/0x01`，只提示“任务未完成”，不回传、不 DEL

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
