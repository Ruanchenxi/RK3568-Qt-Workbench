# Qt Virtual Keyboard 过渡接入方案

Status: Draft  
Owner: Codex  
Last Updated: 2026-03-17  
适用范围：Qt 5.15.2 / Qt Widgets 主体 / RK3588 Ubuntu / QWebEngine 工作台场景下的官方键盘过渡方案。  
不适用范围：不替代长期正式的应用内自定义键盘方案。  

## 1. 目标

在不大规模改动现有 Widgets 业务页的前提下，先提供一条可验证的软键盘过渡路径：

1. 登录页可用
2. 系统设置页可用
3. 工作台页不被误接管
4. 后续切换到自定义键盘时，不要求重写业务页面

## 2. 当前结论

当前推荐把 Qt Virtual Keyboard 视为**过渡层**，不是最终产品方案。

原因：

1. 官方键盘可以较低成本提供基础输入能力
2. 但它不能天然满足“字段分型 + 页面避让 + 工作台严格隔离”的最终产品要求
3. 长期正式方案仍建议回到应用内自定义键盘

## 3. 第一阶段接入策略

### 3.1 启用方式

第一阶段采用：

1. `QT_IM_MODULE=qtvirtualkeyboard`
2. 保持官方桌面集成模式
3. 不引入 QML `InputPanel` 宿主，不做大规模 Qt Quick 改造

### 3.2 页面范围

第一批允许软键盘的页面：

1. `login`
2. `system`

第一批明确禁用软键盘的页面：

1. `workbench`
2. 其余未适配页面默认不作为验证主场

### 3.3 低耦合要求

页面层不要直接依赖 Qt Virtual Keyboard 细节。

应通过统一协调层完成：

1. 启动环境注入
2. 页面白名单控制
3. 工作台黑名单控制
4. 页切换时收起输入法

## 4. 当前代码骨架

新增输入法协调层：

1. `src/app/InputMethodCoordinator.*`
2. `src/app/IInputMethodProvider.h`
3. `src/app/QtVirtualKeyboardProvider.*`
4. `src/app/QtVirtualKeyboardDock.*`

职责：

1. 读取 `input/softKeyboardProvider`
2. 启动前注入 `QT_IM_MODULE`
3. 判断页面是否允许官方键盘
4. 在工作台页禁用输入法能力
5. 页切换到非白名单页时主动收起键盘
6. 通过 Provider 抽象隔离官方键盘实现，为后续切换自定义键盘预留替换点
7. 通过应用内底部宿主把官方键盘限制在主窗口内部

## 5. 配置项

当前过渡方案使用以下配置项：

1. `input/softKeyboardProvider`
   - Linux 默认：`qtvirtualkeyboard`
   - Windows 默认：`none`
2. `input/softKeyboardEnabledPages`
   - 默认：`login,system`
3. `input/qtVirtualKeyboardStyle`
   - 默认：空

环境变量覆盖：

1. `RK3568_SOFT_KEYBOARD_PROVIDER`
   - 可选值：`qtvirtualkeyboard` / `none`

## 6. 已知边界

1. 工作台页仍是 `QWebEngine` 黑盒区，不纳入官方键盘正式适配范围
2. 当前过渡方案只解决“能先用”，不等于“输入体验已经达成正式要求”
3. 页面避让、编辑条、字段分型仍属于后续自定义键盘阶段的主任务

## 7. 后续演进顺序

1. 先验证 Qt Virtual Keyboard 在登录页/系统设置页是否稳定
2. 若工作台误受影响或板端焦点不稳，则把官方键盘定性为临时工具
3. 后续自定义键盘沿用 `InputMethodCoordinator` 的页面边界与宿主抽象，不直接把业务页改成依赖官方键盘
