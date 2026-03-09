# 传票协议开发手册（TICKET PROTOCOL GUIDE）

Status: Active  
Owner: 钥匙链路维护者  
Last Updated: 2026-03-09  
适用范围：主程序中传票 JSON -> payload -> frame(s) -> ACK 续发链路。  
不适用范围：不替代 `SET_COM / Q_TASK / DEL` 的基础钥匙协议文档。  

## 1. 主程序中的归属

### UI

`features/key/ui`

只负责：

1. 系统票数据展示
2. 当前选中票据摘要
3. 传票按钮
4. 状态提示
5. 串口报文 / HTTP 服务端报文展示

### Application

`features/key/application`

负责：

1. HTTP 接收 JSON
2. 系统票入池
3. 票状态管理
4. 手动/自动传票入口编排

### Protocol

`features/key/protocol`

负责：

1. `JSON -> payload`
2. `payload -> frame(s)`
3. 单帧/多帧
4. `5A 83 / 5A 03` ACK 续发
5. 传票 in-flight 状态

## 2. JSON 到 payload 规则

当前主程序复用测试工具中已验证通过的规则：

1. `taskId`：前 8B `uint64 LE`，后 8B `00`
2. `createTime / planTime`：BCD 编码，不足位补 `FF FF`
3. 头区固定 `160B`
4. `stepTableStart = p[160]`
5. 每步固定 `8B`
6. `stringAreaStart = 160 + 8 * stepNum`
7. `displayOff[i] / taskNameOff / fileSize` 动态回填
8. 字符串使用 `GBK + 00`

## 3. 外层帧规则

1. 单帧/最后帧：`Cmd=0x03`
2. 还有后续帧：`Cmd=0x83`
3. `Len = Cmd(1) + Seq(2) + payloadChunk`
4. `CRC` 覆盖 `Cmd + Seq + payloadChunk`
5. `KeySerialClient` 统一负责多帧 cursor

## 4. ACK 续发规则

1. `ACK(0x5A)` 的 `Data[0]` 代表被确认的命令码  
2. 传票多帧时：
   - 收到 `5A 83`：继续发下一帧
   - 收到最后一帧 ACK（通常 `5A 03`）：整组发送完成
3. 超时 / NAK / 首帧失败 / 后续帧失败都必须回写系统票状态

## 5. 当前主程序内状态

系统票 `transferState`：

1. `received`
2. `auto-pending`
3. `sending`
4. `success`
5. `failed`
6. `key-cleared`

状态说明：

1. `received`：主程序已接收工作台 JSON，尚未开始传票  
2. `auto-pending`：等待串口连接、钥匙就绪或 `SET_COM` 握手完成后自动传票  
3. `sending`：正在向钥匙发送传票  
4. `success`：已传票到钥匙，等待设备后续执行  
5. `failed`：传票失败，请结合串口报文定位原因  
6. `key-cleared`：钥匙中的同任务已被手动删除，可再次传票  

## 6. 当前关键业务规则

1. 同一 `taskId` 已处于 `sending/success` 时，默认不重复下发  
2. 如果钥匙中的对应任务已经被手动删除（DEL 成功并经 Q_TASK 验证消失）：
   - 系统票会进入 `key-cleared`
   - 此时允许再次手动传票
   - 再次从工作台触发“钥匙传票”时，也允许重新进入自动传票链  
3. 工作台撤销后的系统票生命周期规则已拍板，但当前故意延后，不在传票协议层实现  

## 7. 当前已验证样本（来自测试工具）

1. 停电单步  
2. 停电双步 `344B`  
3. 停电双步（长 taskName）`350B`  
4. 停电三步 `412B`  
5. 送电单步 `266B`  
6. 送电双步 `310B`  

说明：

1. 这些是编码规则基线  
2. 主程序真机链路仍需继续验证  

## 8. 当前阶段重点

1. 传票主链已基本打通  
2. 当前下一阶段重点是 **回传链路**  
3. 再往后是 `初始化 / 下载 RFID / 回传` 主链继续完善  
4. 撤销/删除系统票等业务 bug 在主链稳定后统一收口  
