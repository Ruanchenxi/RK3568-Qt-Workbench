# 配置项参考（CONFIG REFERENCE）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-25
适用范围：说明当前项目关键配置项、默认值及其影响范围。  
不适用范围：不描述完整协议细节。  

## 1. 系统与工作台

| Key | 默认值 | 说明 |
|---|---|---|
| `system/homeUrl` | `http://localhost/pad` | 工作台首页 URL |
| `system/apiUrl` | `http://localhost/api/kids-outage/third-api` | 认证/业务相关 API 基地址 |
| `system/stationId` | `001` | 当前业务站号；`0` 表示全站模式，当前会同时影响 `DEL`、`INIT/package-data` 与 `DN_RFID/rfid-data` |
| `system/tenantCode` | `000000` | 租户编码 |
| `auth/accountListUrl` | 空字符串 | 登录页“选择账号”接口完整地址；为空时回退到 `system/apiUrl + /list-account` |
| `auth/cardLoginUrl` | 空字符串 | 刷卡登录接口完整地址；为空时回退到 `system/apiUrl + /login-by-card` |
| `system/userListUrl` | 空字符串 | 用户身份采集页“刷新用户”接口完整地址；为空时回退到 `system/apiUrl + /list-user` |
| `system/updateCardNoUrl` | 空字符串 | 用户身份采集页“采集卡号”保存接口完整地址；为空时回退到 `system/apiUrl + /update-card-no` |

## 1.1 键盘相关

| Key | 默认值 | 说明 |
|---|---|---|
| `input/softKeyboardProvider` | `none` | 旧的官方输入法/官方键盘过渡路径遗留配置。当前程序已不再读取它来驱动主线输入法。 |
| `input/softKeyboardEnabledPages` | `login,system` | 历史页面白名单遗留配置，当前只保留兼容口径，不再参与主线路由。 |
| `input/qtVirtualKeyboardStyle` | 空字符串 | 旧官方键盘样式遗留项，当前主线不再使用。 |

补充说明：

1. 当前自定义 QWidget 键盘的页面级高度策略仍是代码内置策略，不走配置项：
   - 登录页：`252`
   - 系统设置页：`260`
   - 其它默认：`256`
2. 当前主线统一触发方式：
   - 先点击可编辑输入框
   - 再点击顶部 `虚拟键盘` 按钮
3. 当前工作台页不纳入这条主线。
4. 以上 `input/*` 配置项只对应旧 Qt Virtual Keyboard 过渡链，不代表当前正式主线。
5. `ui/keyboarddialog.ui` 属于历史 dialog 键盘方案：
   - 不由上述配置项驱动
   - 不属于当前正式键盘主线
   - 已退出工程主线，不再属于当前正式构建
   - 这里只是历史口径说明，不代表旧 Qt Virtual Keyboard 整链也曾经被这些配置项驱动过
   - `ui/mainwindow.ui` 的 `keyboardPage` 历史壳页已清理，不再保留为当前路由
6. 旧 Qt Virtual Keyboard 过渡链已从当前主构建中退出，相关源码与资源不再装配进主线。
7. 这组配置项只用于说明历史兼容关系，不等于当前主线仍在依赖旧链。
8. 日常维护时，若当前任务不是“历史配置核对”专项，默认可忽略上述旧 provider 配置项。
9. 上述 `input/*` 配置项的存在，不代表它们还会被当前程序消费；它们只保留为历史兼容说明。
10. 当前不要把这些遗留配置误读成“主线仍在依赖旧官方输入法链”。

补充说明：

1. `resources/inputmethod.qrc` 已退出主构建，不再作为当前主线资源装配项。
2. `src/app/QtVirtualKeyboardProvider.*` 与 `src/app/QtVirtualKeyboardDock.*` 已退出当前编译清单并从工程中删除。
3. `src/app/InputMethodCoordinator.*` 仍保留在源码树中，作为历史评估壳与删除前条件参考，不再驱动旧输入法行为。

## 1.2 旧链删除前核对

以下配置项现在只用于说明历史兼容口径，不再参与当前主线实现：

| Key | 现阶段口径 | 说明 |
|---|---|---|
| `input/softKeyboardProvider` | `none` | 当前程序不再根据它选择旧 provider。 |
| `input/softKeyboardEnabledPages` | `login,system` | 当前程序不再根据它决定键盘路由。 |
| `input/qtVirtualKeyboardStyle` | 空字符串 | 当前程序不再读取它来加载官方键盘样式。 |

NOTE: 这组配置项只用于说明历史兼容关系，不等于“旧链整链清理完成”；当前只是退出主构建，源码树仍保留历史评估壳。

## 2. 串口

| Key | Windows 默认值 | Linux/RK3588 默认值 | 说明 |
|---|---|---|---|
| `serial/keyPort` | 空字符串 | `/dev/ttyS4` | 钥匙链路串口 |
| `serial/cardPort` | 空字符串 | `/dev/ttyS3` | 刷卡链路串口 |
| `serial/baudRate` | `115200` | `115200` | X5 钥匙底座固定波特率 |
| `serial/dataBits` | `8` | `8` | 数据位 |
| `serial/replayEnabled` | `false` | `false` | 是否启用回放串口 |
| `serial/replayScript` | `test/replay/scenario_half_and_sticky.jsonl` | 同左 | 回放脚本路径；多帧回传回放可设为 `test/replay/scenario_up_task_log_multiframe.jsonl` |

## 2.1 服务启动与日志

| Key | Windows 默认值 | Linux/RK3588 默认值 | 说明 |
|---|---|---|---|
| `advanced/autoStart` | `true` | `true` | 进入服务日志页后是否自动进入服务治理流程 |
| `service/startMode` | `local` | `remote` | 默认启动口径；当前仍以“服务健康就不重启”作为默认策略 |
| `service/remoteFallbackLocal` | `true` | `true`（检测到本地恢复脚本时） | `remote` 检查失败后是否回退执行本地脚本恢复 |
| `service/workDir` | 应用目录 | `/home/linaro/outage`（存在时） | 本地恢复脚本的工作目录；当前实现还会从应用目录向上搜索带有服务脚本/日志的祖先目录 |
| `service/scriptPath` | `start-all.bat` | `/home/linaro/outage/start.sh`（存在时） | 本地恢复脚本路径；生产板端推荐绝对路径 |
| `service/remoteRequirePorts` | `true` | `true`（检测到本地恢复脚本时） | `remote` 健康检查是否要求关键端口一并可达 |
| `service/remoteRequiredPorts` | `8081,9000` | `8081`（命中 `/home/linaro/outage/start.sh` 时） | `remote` 模式下的关键端口 |
| `service/autoCleanPorts` | `false` | `false` | 检测到端口占用时是否自动清理冲突进程 |
| `service/forceRestartScript` | `false` | `false` | 是否忽略现有服务状态，强制清端口并重新执行服务脚本；默认关闭，仅作为应急开关 |
| `service/logFiles` | 空字符串 | 空字符串 | 外部日志文件列表；为空时程序会尝试自动识别 `service_startup.log`、板端原产品日志等真实服务侧日志 |
| `service/externalLogTailEnabled` | `true` | `true` | 是否跟踪外部日志文件 |
| `service/externalLogTailIntervalMs` | `1000` | `1000` | 外部日志轮询间隔 |

补充说明：

1. 当前默认策略已恢复为更接近原产品的口径：
   - 服务健康时不重启脚本
   - 服务异常时，再按 `startMode / remoteFallbackLocal` 决定是否恢复
2. 若手动开启 `forceRestartScript=true`：
   - 进入服务日志页后，Windows / Linux 都会优先尝试“清端口 -> 重启脚本”
   - 该策略只建议用于临时联调或现场恢复，不建议作为长期默认策略
3. 在 `forceRestartScript=true` 下：
   - `service/autoCleanPorts` 会被运行时强制视为开启
   - Windows 端即使无法识别 PID 对应的进程名，也会继续按 PID 尝试 `taskkill`
4. 当前外部日志自动识别优先级已收口为“真实服务日志优先”，包括：
   - `service_startup.log`
   - `/home/linaro/outage/target/kids/log/info-当天.log`
   - `/home/linaro/outage/target/kids/log/error-当天.log`
   - `/home/linaro/AdapterService/bin/log-当月.txt`
5. Windows 本地联调下，不再默认把前端 `logs/app.log` 当成服务日志来源。
6. 当前工作目录 / 脚本 / 日志路径策略是：
   - 显式配置优先：`service/workDir` / `service/scriptPath` 优先级最高
   - 板端固定路径优先：Linux 若存在 `/home/linaro/outage/start.sh`，默认优先采用
   - 自动搜索兜底：若显式路径不可用，再从应用目录逐级向上搜索祖先目录
7. 这样做的原因是：
   - 板端部署时用固定路径最稳
   - Windows 联调时保留自动搜索，避免因为 build 目录层级变化导致脚本和日志失联
8. Linux 板端命中 `/home/linaro/outage/start.sh` 时，当前实现会跳过前置端口治理：
   - 不再额外检查/清理 `9000/9001`
   - 直接交给原产品脚本自行处理 `8081`
9. 当前原产品日志默认识别优先级包括：
   - `/home/linaro/outage/target/kids/log/info-当天.log`
   - `/home/linaro/outage/target/kids/log/error-当天.log`
   - `/home/linaro/AdapterService/bin/log-当月.txt`
10. Linux 板端的“原产品方法”当前应理解为：
    - 固定脚本：`/home/linaro/outage/start.sh`
    - 固定工作目录：`/home/linaro/outage`
    - 固定主日志目录：`/home/linaro/outage/target/kids/log`
    - 自动搜索只保留给 Windows 本地联调用作兜底

## 3. 传票 HTTP 输入源

| Key | 默认值 | 说明 |
|---|---|---|
| `ticket/httpIngressHost` | `127.0.0.1` | 本地 HTTP 接收监听地址 |
| `ticket/httpIngressPort` | `8888` | 本地 HTTP 接收监听端口 |
| `ticket/httpIngressPath` | `/ywticket/WebApi/transTicket` | 工作台传票 JSON 的接收路径 |
| `ticket/autoTransferEnabled` | `true` | 是否自动传票；当前默认开启，但仍受串口/钥匙/握手状态约束 |
| `ticket/httpReturnUrl` | 空字符串 | 钥匙回传 HTTP 接口完整地址；为空时回退到 `system/apiUrl + /finish-step-batch` |
| `ticket/debugFrameChunkSize` | `0` | 调试用强制分帧大小；设为 `64/80/...` 可在本地强制触发多帧传票，`0` 表示关闭 |
| `key/backendStationNo` | `0` | 历史兼容镜像字段；当前主线以 `system/stationId` 为真值，保存系统设置时会同步写入本键 |

说明：

1. 首期接入阶段，工作台 JSON 通过本地 HTTP 方式进入主程序。  
2. 该链路支持跨域与 `OPTIONS` 预检。  
3. 启动日志会打印当前真实 `host / port / path`，用于联调确认。  
4. `autoTransferEnabled` 仅表示是否尝试自动触发传票，不会改变底层协议规则。  
5. Windows 下这些键由 `QSettings("RK3568", "DigitalPowerSystem")` 持久化；调试时可通过注册表路径：
   - `HKCU\Software\RK3568\DigitalPowerSystem\serial`
   - `HKCU\Software\RK3568\DigitalPowerSystem\ticket`

## 3.1 刷卡登录补充

1. 当前刷卡登录默认遵循原产品抓包真值：
   - 串口路径：`/dev/ttyS3`
   - 串口参数：`9600 8-N-1`
   - 帧长：`8` 字节
   - 示例帧：`AA 31 08 00 49 A7 E0 06`
2. 当前实现采用流式解析，不把一次 `readyRead` 视作一整条刷卡消息。
3. 当前上传给后端的字段是：
   - `cardNo`
   - `tenantId`
4. 当前 `cardNo` 采用原产品实际请求口径：
   - `080049a7e006`
5. 当前同一卡号会做短时间去重，避免卡持续贴在读卡器上时重复触发登录。
6. 用户身份采集页当前也会复用同一条读卡串口真值：
   - `/dev/ttyS3`
   - `9600 8-N-1`
   - 8 字节定长帧
7. 用户身份采集页当前保存卡号接口请求体按后端文档固定为：
   - `{"userId":"...","cardNo":"..."}`
8. 用户身份采集页当前默认尝试从 `list-user` 解析用户详情；若接口未返回 `userId`，页面会提示“无法采集卡号”。
9. Linux 板端当前补了一次读卡串口迁移：
   - 若历史配置仍是 `/dev/ttyACM0`
   - 且板端存在 `/dev/ttyS3`
   - 程序会自动把 `serial/cardPort` 迁到 `/dev/ttyS3`
10. 当前 `list-user / update-card-no` 认证头按后端文档使用：
    - `kids-auth: bearer <accessToken>`
    - `kids-requested-with: KidsHttpRequest`

补充说明（2026-03-23）：

1. 当前主线统一入口是顶部 `虚拟键盘` 按钮。
2. 登录页 / 系统设置页可先聚焦输入框再打开，也允许先打开后再落到输入目标。
3. 工作台页走特例 bridge：必须先让网页焦点落在可编辑元素上。
4. 钥匙管理页 / 服务日志页当前不作为有效输入页。
5. 跨页切换时，主窗口会统一收起键盘并清空上一个页面的输入上下文。
6. `TicketIngressService` 当前会把“HTTP 收到请求”和“业务真实入池成功”区分开。
7. 当前只有在系统票真实入池成功时，HTTP 才返回：
   - `success=true`
8. 当前本地 HTTP 接票入口的错误语义已收紧：
   - 错路径：`404`
   - 错方法：`405`
   - 请求行非法 / `Content-Length` 非法 / JSON 无法入池：`400`
9. 若请求被业务接受但按规则忽略（例如重复任务），HTTP 仍可返回 `success=true`，但 `msg` 必须体现真实业务语义。
