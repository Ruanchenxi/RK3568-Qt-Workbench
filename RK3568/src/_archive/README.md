# _archive 目录说明

本目录用于存放**历史遗留模块**，原则是：

1. 不参与主工程编译。
2. 不作为新功能开发依赖。
3. 仅用于追溯历史实现和紧急回滚。

## 当前归档模块

1. `services/AuthService.*`
2. `services/SerialService.*`
3. `protocol/KeyCabinetProtocol.*`
4. `protocol/ProtocolBase.h`

## 为什么归档

1. 主链路已统一为 `KeySerialClient`（7E6C 协议）。
2. 遗留模块继续留在主路径会增加误引用/误编译风险。
3. 归档保留了回滚能力，同时让当前工程结构更清晰。

## 恢复方法（仅在确有需要时）

1. 将对应文件恢复到当前目录树下的目标位置（不要恢复到已废弃平铺目录）：
   - 登录相关：`src/features/auth/infra/password/`
   - 串口平台：`src/platform/serial/`
   - 钥匙协议：`src/features/key/protocol/`
2. 在 `RK3568.pro` 中恢复 `SOURCES/HEADERS` 条目。
3. 全量编译并执行回归测试，确认没有符号冲突或行为回退。
