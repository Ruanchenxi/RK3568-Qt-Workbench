# 键盘模块分层设计

Status: Active  
Owner: Codex  
Last Updated: 2026-03-19  
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
2. 旧 Qt Virtual Keyboard 过渡路径默认关闭
3. 第一阶段只接：
   - 登录页
   - 系统设置页
4. 统一触发方式：
   - 先点击输入框
   - 再点击顶部 `虚拟键盘` 按钮
5. 当前工作台页不并入这条主线，后续如要支持，按特例方案单独设计

### 2.1 遗留链清理准备口径

当前针对旧键盘 / 旧输入法链，统一按下面口径理解：

1. `ui/keyboarddialog.ui`
   - 属于历史 dialog 键盘方案
   - 不属于当前正式主线
   - 当前已移出工程主线构建清单
   - 当前仅保留磁盘文件，等待后续按清理候选统一评估
   - 当前应视为优先删除候选，而不是继续维护的 UI 入口
2. 旧 Qt Virtual Keyboard 过渡链当前默认关闭，但暂不直接删除，范围主要包括：
   - `src/app/InputMethodCoordinator.*`
   - `src/app/QtVirtualKeyboardProvider.*`
   - `src/app/QtVirtualKeyboardDock.*`
   - `src/app/qml/InputPanelHost.qml`
3. 当前之所以“暂不删”，不是因为旧链仍是主线，而是因为它还挂着清理阻塞项：
   - 启动期输入法环境设置
   - 页面白名单 / provider 路由
   - `resources/inputmethod.qrc` 资源装配
   - 删除前的验证基线尚未单独完成
4. 新 agent 在当前阶段不要：
   - 继续扩写 `keyboarddialog.ui`
   - 把新页面重新接回旧 Qt Virtual Keyboard 过渡链
   - 把“默认关闭”误解成“已经完成删除”

### 2.2 日常维护视野边界

对日常页面开发 / 键盘体验微调 / 新功能接入来说，下面这些内容当前默认都不在日常维护主视野内：

1. `ui/keyboarddialog.ui`
2. `src/app/InputMethodCoordinator.*`
3. `src/app/QtVirtualKeyboardProvider.*`
4. `src/app/QtVirtualKeyboardDock.*`
5. `src/app/qml/InputPanelHost.qml`

只有在以下场景下，才应主动进入这组文件：

1. 专门做旧键盘 / 旧输入法清理评估
2. 核对旧 provider 启动入口或资源装配阻塞项
3. 做删除前验证基线确认

除上述场景外，后续 agent 默认应把注意力放在：

1. `src/features/keyboard/`
2. 登录页 / 系统设置页与自定义 QWidget 键盘的现主线接入

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

1. 先点击输入框
2. 再点击顶部 `虚拟键盘` 按钮
3. 键盘显示/收起由 `KeyboardController` 协调

### 4.2 第一阶段支持范围

当前支持：

1. 登录页
2. 系统设置页

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
   - 当前不纳入本地 QWidget 键盘主线

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
3. 不要让键盘模块重新依赖插件输入法方案
4. 不要把工作台页和本地 Widgets 页强行统一成同一套底层实现
5. 不要再把 `ui/keyboarddialog.ui` 当成当前键盘主线继续扩功能，也不要重新加回工程主线构建清单
6. 不要在未完成入口映射与验证基线前，直接删除旧 Qt Virtual Keyboard 过渡链
