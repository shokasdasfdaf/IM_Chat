# 局域网即时通讯工具 — 设计文档

> 创建于 2026-05-19 | 8 天冲刺 Day 3

---

## 一、需求分析

### 1.1 谁用？解决什么问题？

- **目标用户**：局域网内的办公/学习场景，需要快速文字沟通的人
- **核心价值**：不需要互联网、不需要注册账号，打开就能聊天
- **不做什么**：不做注册系统（用户名即身份）、不做群聊、不做文件传输、不做加密

### 1.2 功能清单（MoSCoW）

| 优先级 | 功能 | 说明 |
|--------|------|------|
| **Must have** | 用户登录 | 输入用户名即可登录，同一用户名不能重复登录 |
| **Must have** | 一对一文字聊天 | 选择在线用户 → 发消息 → 对方实时收到 |
| **Must have** | 在线用户列表 | 显示当前在线的所有用户，自动刷新 |
| **Should have** | 聊天记录存储 | SQLite 存储，服务器端持久化 |
| **Should have** | 聊天记录搜索 | 在历史记录中按关键词搜索 |
| **Could have** | 文件传输 | 发送文件给指定用户 |
| **Could have** | Emoji 表情 | 常用表情面板 |
| **Won't have** | 群聊、语音视频、消息加密、用户注册 | 本次不做 |

---

## 二、用例设计

### 用例 1：用户登录

```
1. 用户打开客户端，看到登录窗口
2. 输入用户名，点击"登录"
3. 客户端连接服务器，发送 {"type":"login","username":"张三"}
4. 服务器检查该用户名是否已在线
5. 如果成功 → 服务器返回在线用户列表，客户端进入主窗口
6. 如果失败 → 服务器返回错误原因，客户端弹出提示
```

### 用例 2：一对一聊天

```
1. 用户 A 在在线列表中点击用户 B
2. 右侧聊天区域切换到与 B 的聊天页面
3. 用户 A 输入消息，点击发送或按 Enter
4. 消息通过服务器转发给用户 B
5. 用户 B 的客户端实时显示消息
6. 消息同时存入 SQLite 数据库
```

### 用例 3：查看聊天记录

```
1. 用户点击"聊天记录"按钮
2. 弹出搜索对话框，输入关键词
3. 服务器从 SQLite 查询匹配的消息
4. 结果按时间倒序显示
```

---

## 三、技术选型

| 项目 | 选择 | 原因 |
|------|------|------|
| Qt 版本 | 6.11.0 MSVC2022 | 已安装 |
| 构建系统 | CMake | Qt 6 官方推荐，qmake 已废弃 |
| 界面技术 | QWidget | 传统桌面应用，学习曲线低 |
| 数据库 | SQLite (Qt SQL 模块) | 嵌入式，零配置 |
| 协议格式 | JSON + 4 字节长度头 | 解决 TCP 粘包，人类可读 |
| 编译器 | MSVC 2022 | 已安装 VS 2022 |

**不引入第三方库**，Qt 自带模块已覆盖全部需求。

---

## 四、架构设计

### 4.1 分层架构

```
┌──────────────────────────────┐
│  客户端 UI 层                 │  登录窗口、主窗口、聊天页面、用户列表
├──────────────────────────────┤
│  客户端逻辑层                 │  连接管理、消息收发、状态管理
├──────────────────────────────┤
│  网络层（客户端+服务器共用）  │  JSON 编解码、TCP 粘包处理
├──────────────────────────────┤
│  服务器逻辑层                 │  用户管理、消息路由、历史查询
├──────────────────────────────┤
│  数据层（服务器）             │  SQLite 读写
└──────────────────────────────┘
```

### 4.2 项目目录结构

```
IM_Chat/
├── docs/
│   └── design.md              # 本文档
├── resources/
│   └── icons/                 # 图标（未来添加）
├── src/
│   ├── common/
│   │   ├── protocol.h         # 消息协议定义（JSON 格式）
│   │   └── protocol.cpp
│   ├── server/
│   │   ├── main.cpp           # 服务器入口
│   │   ├── chatserver.h       # 服务器核心
│   │   ├── chatserver.cpp
│   │   ├── clienthandler.h    # 单个客户端连接处理
│   │   └── clienthandler.cpp
│   ├── client/
│   │   ├── main.cpp           # 客户端入口
│   │   ├── ui/
│   │   │   ├── mainwindow.h       # 主窗口
│   │   │   ├── mainwindow.cpp
│   │   │   ├── loginwidget.h      # 登录页面
│   │   │   └── loginwidget.cpp
│   │   ├── chatclient.h       # 客户端网络逻辑
│   │   └── chatclient.cpp
├── tests/                     # 单元测试（未来添加）
├── .gitignore
├── CMakeLists.txt
└── README.md
```

---

## 五、通信协议设计

### 5.1 消息格式

每条消息 = **4 字节长度（大端） + JSON 数据**

```
| 4 bytes (uint32 big-endian) | UTF-8 JSON string |
| 0x0000001C                  | {"type":"login","username":"张三"} |
```

### 5.2 消息类型

#### 客户端 → 服务器

| type | 说明 | JSON 示例 |
|------|------|-----------|
| `login` | 登录请求 | `{"type":"login","username":"张三"}` |
| `chat` | 发送消息 | `{"type":"chat","to":"李四","content":"你好"}` |
| `history` | 查询历史 | `{"type":"history","keyword":"你好"}` |

#### 服务器 → 客户端

| type | 说明 | JSON 示例 |
|------|------|-----------|
| `login_ok` | 登录成功 | `{"type":"login_ok","users":["张三","李四"]}` |
| `login_failed` | 登录失败 | `{"type":"login_failed","reason":"用户名已在线"}` |
| `user_joined` | 新用户上线 | `{"type":"user_joined","username":"王五"}` |
| `user_left` | 用户下线 | `{"type":"user_left","username":"王五"}` |
| `chat` | 收到消息 | `{"type":"chat","from":"张三","content":"你好"}` |
| `history_result` | 历史查询结果 | `{"type":"history_result","messages":[...]}` |

---

## 六、数据库设计（Should have 阶段）

### 表：messages

```sql
CREATE TABLE messages (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    from_user TEXT    NOT NULL,
    to_user   TEXT    NOT NULL,
    content   TEXT    NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## 七、开发阶段

| 阶段 | 内容 | 预计 |
|------|------|------|
| 1 | 协议定义（protocol.h/cpp） | 今天 |
| 2 | 服务器骨架（监听 + accept + 收发） | 今天 |
| 3 | 客户端登录（UI + 连接服务器） | 今天 |
| 4 | 在线用户列表（广播 + 列表刷新） | 明天 |
| 5 | 一对一聊天（消息路由 + 聊天窗口） | 明天 |
| 6 | SQLite 集成（聊天记录存储+查询） | 5/20 |
| 7 | 多线程（moveToThread） | 5/20 |
| 8 | 打包 + Git 提交 | 5/21 |

---

## 八、进度记录

| 日期 | 完成内容 | Git Commit |
|------|----------|------------|
| 5/19 | — | — |
