# 配置项参考（CONFIG REFERENCE）

Status: Active  
Owner: 项目维护者  
Last Updated: 2026-03-17  
适用范围：说明当前项目关键配置项、默认值及其影响范围。  
不适用范围：不描述完整协议细节。  

## 1. 系统与工作台

| Key | 默认值 | 说明 |
|---|---|---|
| `system/homeUrl` | `http://localhost/pad` | 工作台首页 URL |
| `system/apiUrl` | `http://localhost/api/kids-outage/third-api` | 认证/业务相关 API 基地址 |
| `system/stationId` | `001` | 当前站号；影响 `DEL`，后续可作为传票站号默认值 |
| `system/tenantCode` | `000000` | 租户编码 |
| `auth/accountListUrl` | 空字符串 | 登录页“选择账号”接口完整地址；为空时回退到 `system/apiUrl + /list-account` |

## 1.1 键盘相关

| Key | 默认值 | 说明 |
|---|---|---|
| `input/softKeyboardProvider` | `none` | 旧的官方输入法/官方键盘过渡路径 Provider。当前主线默认关闭，避免影响自定义 QWidget 键盘稳定性。 |
| `input/softKeyboardEnabledPages` | `login,system` | 历史软键盘页面白名单配置，当前主要保留兼容和试验用途。 |
| `input/qtVirtualKeyboardStyle` | 空字符串 | 仅在重新启用官方键盘路径时生效。当前自定义 QWidget 键盘主线不依赖此项。 |

补充说明：

1. 当前自定义 QWidget 键盘的页面级高度策略仍是代码内置策略，不走配置项：
   - 登录页：`252`
   - 系统设置页：`260`
   - 其它默认：`256`
2. 当前主线统一触发方式：
   - 先点击可编辑输入框
   - 再点击顶部 `虚拟键盘` 按钮
3. 当前工作台页不纳入这条主线。

## 2. 串口

| Key | Windows 默认值 | Linux/RK3588 默认值 | 说明 |
|---|---|---|---|
| `serial/keyPort` | 空字符串 | `/dev/ttyS4` | 钥匙链路串口 |
| `serial/cardPort` | 空字符串 | `/dev/ttyS3` | 刷卡链路串口 |
| `serial/baudRate` | `115200` | `115200` | X5 钥匙底座固定波特率 |
| `serial/dataBits` | `8` | `8` | 数据位 |
| `serial/replayEnabled` | `false` | `false` | 是否启用回放串口 |
| `serial/replayScript` | `test/replay/scenario_half_and_sticky.jsonl` | 同左 | 回放脚本路径；多帧回传回放可设为 `test/replay/scenario_up_task_log_multiframe.jsonl` |

## 3. 传票 HTTP 输入源

| Key | 默认值 | 说明 |
|---|---|---|
| `ticket/httpIngressHost` | `127.0.0.1` | 本地 HTTP 接收监听地址 |
| `ticket/httpIngressPort` | `8888` | 本地 HTTP 接收监听端口 |
| `ticket/httpIngressPath` | `/ywticket/WebApi/transTicket` | 工作台传票 JSON 的接收路径 |
| `ticket/autoTransferEnabled` | `true` | 是否自动传票；当前默认开启，但仍受串口/钥匙/握手状态约束 |
| `ticket/httpReturnUrl` | 空字符串 | 钥匙回传 HTTP 接口完整地址；为空时回退到 `system/apiUrl + /finish-step-batch` |
| `ticket/debugFrameChunkSize` | `0` | 调试用强制分帧大小；设为 `64/80/...` 可在本地强制触发多帧传票，`0` 表示关闭 |
| `key/backendStationNo` | `0` | 初始化 / 下载 RFID 后端取数使用的业务站号；`0` 表示按后端约定请求全站聚合数据 |

说明：

1. 首期接入阶段，工作台 JSON 通过本地 HTTP 方式进入主程序。  
2. 该链路支持跨域与 `OPTIONS` 预检。  
3. `autoTransferEnabled` 仅表示是否尝试自动触发传票，不会改变底层协议规则。  
4. Windows 下这些键由 `QSettings("RK3568", "DigitalPowerSystem")` 持久化；调试时可通过注册表路径：
   - `HKCU\Software\RK3568\DigitalPowerSystem\serial`
   - `HKCU\Software\RK3568\DigitalPowerSystem\ticket`
