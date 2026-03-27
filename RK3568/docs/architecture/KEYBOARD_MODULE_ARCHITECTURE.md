# 键盘模块分层设计

Status: Active  
Owner: Codex  
Last Updated: 2026-03-20
适用范围：Qt Widgets 主体项目中的自定义键盘模块设计与接入边界。  
不适用范围：不替代工作台 QWebEngine 的后续特例接入方案。  

## 1. 目标

键盘模块必须满足：

1. 高解耦
2. 高可读性
3. 高扩展性
4. 后续 UI 可重做而不牵动业务接口

因此键盘不能继续作为页面内的临时功能堆叠，而必须按项目现有架构纪律做正式分层。

## 2. 当前主线结论

当前正式主线已经切换为：

1. 本地 Widgets 页使用自定义 QWidget 键盘
2. 旧 Qt Virtual Keyboard 过渡链已从工程主线退出，不再参与构建
3. 当前页面能力按分层处理：
   - 登录页：完整支持
   - 系统设置页：完整支持
   - 工作台页：仅在网页当前焦点位于可编辑元素时，允许通过特例 bridge 输入
   - 钥匙管理页：当前不作为有效输入页
   - 服务日志页：当前不支持
4. 统一触发方式：
   - 顶部 `虚拟键盘` 按钮是统一入口
   - 本地输入页可先聚焦输入框再打开，也允许先打开后再落到输入目标
   - 工作台页必须先让网页焦点落在可编辑元素上
5. 跨页切换时，主窗口统一收起键盘并清空上一个页面的输入上下文

### 2.1 退场结果

旧 Qt Virtual Keyboard 过渡链的当前结论是：

1. 已从工程主构建中移除
2. `RK3568.pro` 不再装配 `resources/inputmethod.qrc`
3. `src/app/app.pri` 不再编译 `QtVirtualKeyboardProvider.*` 和 `QtVirtualKeyboardDock.*`
4. `InputMethodCoordinator.*` 仍保留在源码树中作为历史评估壳，不再进入当前主构建
5. `src/app/qml/InputPanelHost.qml` 与 `resources/inputmethod.qrc` 已退出工程装配，不再承担主线职责

补充说明（2026-03-20）：

1. `ui/mainwindow.ui` 里的 `keyboardPage` 只是历史占位页，不代表旧 Qt Virtual Keyboard 整链已经“清理完成”
2. `PAGE_KEYBOARD` 历史路由已清理，当前正式主线仍是自定义 QWidget 键盘
3. 后续如需对外表述，只能写成“旧过渡链已退出当前主构建，保留历史壳页与删除前条件”，不要再写成“整链清理完成”

### 2.2 维护边界

对日常页面开发 / 键盘体验微调 / 新功能接入来说，当前主视野应聚焦于：

1. `src/features/keyboard/`
2. 登录页 / 系统设置页的完整键盘主线，以及工作台页的特例 bridge
3. 不再让旧 Qt Virtual Keyboard 过渡链影响主线设计、实现与评审结论

## 3. 分层

### 3.1 domain

位置：

1. `src/features/keyboard/domain/`

职责：

1. 表达键盘模式等纯规则类型
2. 不依赖 QWidget 细节

当前内容：

1. `KeyboardTypes.h`

### 3.2 application

位置：

1. `src/features/keyboard/application/`

职责：

1. 管理当前输入目标
2. 解析当前输入框对应的键盘模式
3. 协调键盘显示/收起
4. 桥接页面与键盘宿主

当前内容：

1. `KeyboardController.*`
2. `KeyboardModeResolver.*`
3. `KeyboardTargetAdapter.*`
4. `KeyboardPagePolicy.*`
5. `KeyboardSizingPolicy.*`
6. `KeyboardPinyinEngine.*`

说明：

1. 第一阶段主要输入目标仍然是 `QLineEdit`
2. 当前已补基础目标识别：
   - `QComboBox::lineEdit()`
3. 后续如扩展 `QTextEdit` 或工作台 bridge，应优先继续放在 application / infra 层

### 3.3 ui

位置：

1. `src/features/keyboard/ui/`

职责：

1. 只负责键盘界面与视觉
2. 不负责页面业务规则
3. 不负责字段判型

当前内容：

1. `KeyboardContainer.*`
2. `NormalKeyboard.*`
3. `NumKeyboard.*`
4. `UrlKeyboard.*`
5. `SymbolKeyboard.*`
6. `CandidateBarWidget.*`
7. `KeyboardTheme.h`

说明：

1. 后续如果重做视觉主题，优先修改本层
2. 不应把页面避让、工作台特例、字段规则重新写回本层

## 4. 当前接入规则

### 4.1 触发方式

当前统一交互：

1. 顶部 `虚拟键盘` 按钮是统一入口
2. 登录页 / 系统设置页可先聚焦输入框再打开，也允许先打开后再落输入目标
3. 工作台页必须先让网页焦点落在可编辑元素上，随后再打开键盘
4. 键盘显示/收起由 `KeyboardController` 协调，跨页切换时由主窗口统一收口

### 4.2 第一阶段支持范围

当前支持：

1. 登录页
2. 系统设置页
3. 工作台页（仅特例 bridge，需先聚焦网页输入元素）

当前正式支持：

1. `QLineEdit`

当前已具备基础目标识别：

1. `QComboBox::lineEdit()`

但串口字段当前已改为：

1. 自动识别 + 下拉选择
2. 不再作为自由文本输入主战场

### 4.3 页面避让

当前策略：

1. 登录页：
   - 不做整页位移
   - 保持双栏布局稳定
2. 系统设置页：
   - 仅允许 `formScrollArea` 局部滚动
   - 当前输入框尽量保持在键盘上方可见区
3. 工作台页：
   - 走网页特例 bridge，不做整页缩放/避让联动
4. 钥匙管理 / 服务日志页：
   - 当前不作为有效输入页

### 4.4 当前模式能力

当前已具备：

1. 普通全字符键盘
2. 数字键盘
3. URL 键盘
4. 模式闭环切换：
   - `URL -> ABC -> 123 -> URL`
   - `123 -> ABC -> 123`
5. 键盘打开状态下切换字段时自动换模式

### 4.5 当前中文输入形态

当前拼音输入已按“固定候选栏”方案收口：

1. 键盘总高度固定
2. 顶部固定一条候选栏，不再因候选出现动态撑高键盘
3. 中文模式下：
   - 候选词显示在固定候选栏
4. 非中文或无候选时：
   - 候选栏保留固定占位
   - 不再显示多余提示标题

## 5. 当前视觉策略

当前键盘 UI 已收口到统一主题：

1. 宿主容器外圆角：`20px`
2. 键帽内圆角：`16px`
3. 键帽填充：`4px`
4. 统一间距：`8px`
5. 阴影：
   - 颜色：`#9CA4AC`
   - 透明度：约 `30%`
   - `blur: 16`
6. 页面级高度策略：
   - 登录页：`252`
   - 系统设置页：`260`
   - 其它默认：`256`

## 6. 后续扩展方向

### 6.1 输入目标扩展

后续可按顺序扩展：

1. `QTextEdit / QPlainTextEdit`
2. 工作台特例 bridge

### 6.2 模式扩展

后续可扩展：

1. 密码专用键盘
2. IP 专用键盘
3. 拼音候选栏分页/视觉深化
4. 候选 preedit 更完整显示

### 6.3 视觉扩展

后续 UI 重做时，应优先改：

1. `ui/` 层样式
2. 键帽尺寸与布局
3. 颜色、字体、圆角、阴影

不应优先改：

1. `application/` 层逻辑
2. 页面业务层

## 7. 当前明确禁止事项

1. 不要再把键盘逻辑散写到各页面里
2. 不要把页面避让写成主窗口统一位移
3. 不要把旧 Qt Virtual Keyboard 过渡链重新接回主构建
4. 不要为了“看起来全页面都能弹”而让无输入目标页面进入有效输入态
5. `ui/keyboarddialog.ui` 只是历史遗留，不要重新恢复到工程中，也不要再把它当成当前键盘主线继续扩功能
6. 不要把 `QtVirtualKeyboardProvider.*`、`QtVirtualKeyboardDock.*` 或 `resources/inputmethod.qrc` 再并回主线构建
7. 不要把 `InputMethodCoordinator.*` 误写成“已整链删除完成”；它当前只是保留在源码树中的历史评估壳
