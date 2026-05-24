# IM_Chat 项目代码审查笔记（面试准备）

---

## 1. 项目架构总览

```
src/
├── common/
│   ├── protocol.h / .cpp        # 协议层：粘包处理、消息构建
│   └── DatabaseWorker.h / .cpp  # 异步数据库层
├── server/
│   ├── chatserver.h / .cpp      # 服务端核心：用户管理、消息路由
│   └── clienthandler.h / .cpp   # 单连接处理器：协议解析、心跳
└── client/
    ├── chatclient.h / .cpp      # 客户端核心：心跳检测、自动重连
    └── ui/
        ├── loginwidget.h / .cpp  # 登录界面
        └── mainwindow.h / .cpp   # 聊天主界面
```

---

## 2. protocol.h / protocol.cpp — 协议层

### 核心设计：4 字节大端长度头 + JSON body

**发送流程：**
1. `QJsonDocument::toJson()` 将 JSON 序列化为 UTF-8 字节数组
2. `QDataStream` 写入 4 字节大端长度头
3. 拼接长度头和数据体，一次 `write()` 发出

**接收流程（粘包处理核心）：**
```
while (buffer.size() >= 4) {
    读前 4 字节 → 长度 L
    if (buffer.size() < 4 + L) break;   // 数据未到齐，等下次 readyRead
    取 [4, 4+L) 为一条完整消息
    删除 buffer 前 4+L 字节
    解析 JSON → 加入结果列表
}
```

**面试要点：**
- 为什么大端？网络字节序（Network Byte Order）标准就是大端，`htonl()` 也是转大端，跨语言/跨平台一致
- `thread_local QHash<QTcpSocket*, QByteArray> s_buffers` — 每个线程独立缓冲区，零锁开销。`thread_local` 确保线程安全
- 用 socket 指针做 key，每个连接有自己独立的缓冲区

**潜在问题：**
- `receiveMessages` 里 JSON 解析失败静默丢弃，没有日志。生产环境应加告警，防止恶意数据包
- `s_buffers` 用裸指针做 key，如果 socket 被 delete 后新 socket 分配到同一地址，可能读到旧缓冲区（重连 bug 的来源——已通过 `resetBuffer()` 修复）
- **面试延伸：** 与 HTTP 的 `Content-Length` 头、WebSocket 的帧头做类比，本质都是帧同步问题

---

## 3. clienthandler.h / clienthandler.cpp — 服务端连接处理器

### 核心设计：每个连接一个实例，用信号槽上报业务事件

**消息分发：**
```
LOGIN   → emit loginRequest(username)
CHAT    → emit chatMessage(to, content)
HISTORY → emit historyRequest(keyword)
PING    → 直接回复 PONG（不经过 ChatServer）
```

**面试要点：**
- PING/PONG 在 `ClientHandler` 层直接处理，不经过 `ChatServer`。原因：心跳是高频操作，减少主线程负担，业务层不需要感知心跳细节
- 用信号槽解耦协议解析和业务逻辑：`ClientHandler` 只负责"这条消息是什么类型"，`ChatServer` 负责"这条消息怎么处理"
- `m_buffer` 成员变量已废弃不用（粘包缓冲移到了 `Protocol` 层），属于残留代码

**潜在问题：**
- **没有校验字段是否存在。** 如果收到 `{"type":"login"}` 没有 username，`msg["username"].toString()` 返回空字符串，`m_users[""]` 造成幽灵用户。生产环境需要对每个必要字段做存在性检查
- `onDisconnected()` 直接 emit 信号，依赖 `ChatServer::onClientDisconnected()` 做清理。如果信号连接被断开，资源泄漏

---

## 4. chatserver.h / chatserver.cpp — 服务端核心

### 核心设计：QHash 维护在线用户，信号槽驱动业务逻辑

**登录处理（关键——重连竞态修复）：**
```cpp
if (m_users.contains(username)) {
    auto *oldHandler = m_users[username];
    m_clients.removeOne(oldHandler);
    m_users.remove(username);
    oldHandler->setUsername(QString());  // ★ 关键：清空用户名
    oldHandler->deleteLater();
}
```

**面试要点：**
- **为什么踢旧连接而不是拒绝？** 这是实际踩过的坑。客户端重连时新 TCP 连接比旧连接的 RST 先到服务端，服务端看到旧用户"还在线"，拒绝新连接会导致重连永远失败。踢旧连接更鲁棒
- **`oldHandler->setUsername(QString())` 为什么要清空？** `deleteLater()` 不是立即删除，而是在事件循环下一轮才删。在删除前 `onClientDisconnected` 可能被触发，不清空用户名会导致误广播 `USER_LEFT`，其他客户端看到该用户"离开了"，实际上他只是重连了
- 用 `QHash<QString, ClientHandler*>` 做 O(1) 的用户名→连接查找，`QList<ClientHandler*>` 做广播遍历

**异步数据库调用：**
```cpp
QMetaObject::invokeMethod(m_dbWorker, "saveMessage", Qt::QueuedConnection,
                          Q_ARG(QString, from), Q_ARG(QString, to), Q_ARG(QString, content));
```
- `Qt::QueuedConnection` 保证槽函数在 `DatabaseWorker` 所在线程（子线程）执行
- 参数通过 `Q_ARG` 传递，Qt 会自动做跨线程的值拷贝

**搜索历史的连接管理：**
```cpp
auto conn = std::make_shared<QMetaObject::Connection>();
*conn = connect(m_dbWorker, &DatabaseWorker::historyResult,
                handler, [handler, conn](const QJsonArray &messages) {
    handler->sendJson(Protocol::buildHistoryResult(messages));
    QObject::disconnect(*conn);  // 一次性连接，用完即断
});
```

**潜在问题：**
- 如果 `handler` 在 `historyResult` 返回前断开了，lambda 里 `handler->sendJson(...)` 是野指针。应用 `QPointer<ClientHandler>` 保护
- `onLoginRequest` 里的 `m_clients.removeOne(oldHandler)` 是 O(n)，在线用户少时无影响

---

## 5. chatclient.h / chatclient.cpp — 客户端核心（最复杂）

### 核心设计：心跳检测 + 自动重连状态机

**心跳检测：**
```cpp
void ChatClient::onHeartbeat() {
    m_missedPongs++;
    if (m_missedPongs >= 3) {
        m_heartbeatTimer->stop();
        m_socket->disconnectFromHost();  // 主动断连 → 触发重连
        return;
    }
    // 发送 PING
}
```
- 每 10 秒发 PING，连续 3 次未收到 PONG（30 秒内）判定连接断开
- 为什么不用 TCP keepalive？系统默认 2 小时才探测，完全不适用于即时通讯

**断开处理（防重入）：**
```cpp
void ChatClient::onDisconnected() {
    m_heartbeatTimer->stop();
    if (m_reconnecting) return;  // ★ 防重入：abort() 触发的 disconnected 直接忽略
    if (!m_lastUsername.isEmpty() && !m_serverHost.isEmpty()) {
        m_reconnecting = true;
        m_reconnectTimer->start();  // 重复定时器，每 3s 尝试
        emit reconnecting();
    } else {
        emit connectionError("已断开连接");
    }
}
```

**重连执行：**
```cpp
void ChatClient::onReconnect() {
    // 防重复：已在连接中/已连接则跳过
    if (m_socket->state() == ConnectingState || m_socket->state() == ConnectedState)
        return;

    m_reconnectCount++;

    m_reconnecting = true;
    Protocol::resetBuffer(m_socket);  // ★ 清空协议缓冲区（粘包 bug 修复）
    m_socket->abort();                // ★ 硬重置 socket
    m_reconnecting = false;

    m_pendingLogin = m_lastUsername;
    m_socket->connectToHost(m_serverHost, m_serverPort);
}
```

**重连成功的处理：**
```cpp
// 在 onReadyRead() 中 LOGIN_OK 分支：
m_heartbeatTimer->start();
m_reconnectTimer->stop();
m_reconnecting = false;
m_missedPongs = 0;
if (m_reconnectCount > 0) {
    m_reconnectCount = 0;
    emit reconnected();
}
emit loginSuccess(users);
```

### 面试核心：重连方案的 3 版演进（重点讲！）

| 版本 | 问题 | 表现 | 修复 |
|------|------|------|------|
| V1 | `onReconnect()` 调用 `abort()` → 触发 `disconnected` → `onDisconnected()` 再启动重连定时器 | 重连风暴，定时器死循环 | 加 `m_reconnecting` 标志位 |
| V2 | 复用 socket 对象（指针不变），`s_buffers` 残留旧数据 | 重连后无法解析 LOGIN_OK，永远登录失败 | 加 `Protocol::resetBuffer()` |
| V3 | `setSingleShot(true)` 单次定时器，server 没重启就永久错过 | 重连失败后不再尝试 | 去掉单次限制，改为重复定时器 |

**面试要点：**
- `abort()` vs `disconnectFromHost()`：abort 立即关闭不等待，适合重连场景；disconnectFromHost 优雅关闭，适合正常断开
- `m_reconnecting` 是经典的状态机防重入模式，设置→执行→清除三段式
- 为什么创建新 socket 不行？新 socket 的指针变了，但 connect 的信号槽关系也需要重新建立，而且旧 socket 的 `deleteLater()` 时序不可控，导致更隐蔽的问题

**潜在问题：**
- 没有 `errorOccurred` 信号处理。如果 DNS 解析失败等非临时错误，会无限重试
- **面试延伸方案：** 可加指数退避（1s → 2s → 4s → 上限 30s），或区分可恢复/不可恢复错误
- `m_socket->abort()` 在 socket 已是 `UnconnectedState` 时行为未定义（已通过状态检查缓解）

---

## 6. DatabaseWorker.h / DatabaseWorker.cpp — 异步数据库

### 核心设计：QThread + moveToThread 模式

```cpp
// ChatServer 构造：
m_dbWorker = new DatabaseWorker();
m_dbThread = new QThread(this);
m_dbWorker->moveToThread(m_dbThread);
connect(m_dbThread, &QThread::started, m_dbWorker, &DatabaseWorker::initialize);
m_dbThread->start();
```

**面试要点：**
- `moveToThread` 后，`DatabaseWorker` 的所有槽函数都在子线程执行，信号发射是线程安全的
- 跨线程调用用 `QMetaObject::invokeMethod(Qt::QueuedConnection)`，调用被序列化到子线程事件队列，天然线程安全
- **参数化查询（`?` 占位符 + `addBindValue`）** 防止 SQL 注入。用户输入 `"'; DROP TABLE messages; --"` 也不会被执行
- 数据库连接使用命名连接 `"chat_db"`，与默认连接隔离
- `CREATE TABLE IF NOT EXISTS` 幂等建表，每次启动都安全

**潜在问题：**
- `saveMessage` / `searchHistory` 没有检查 `m_db.isOpen()`，如果 `initialize()` 失败（比如目录无写权限），后续数据库操作静默失败
- `searchHistory` 用 `LIKE '%keyword%'` 做模糊搜索，全表扫描，数据量大时性能差
- **面试延伸方案：** SQLite FTS5 全文搜索、分页加载、服务端缓存

---

## 7. UI 层 — LoginWidget / MainWindow

**LoginWidget：**
- 端口 `QIntValidator(1, 65535)`，用户名非空检查
- 登录中禁用按钮防重复点击
- 默认 localhost:8888，方便开发测试

**MainWindow：**
- 用户列表 + 聊天页 `QStackedWidget`，支持多会话切换
- `switchToUser()` 每次重置窗口标题 → 与重连标题更新冲突（已知问题，当前低优先级）
- 聊天记录用 `QTextEdit::append()` 追加，搜索结果用 `setHtml()` 替换

---

## 8. 面试可扩展方向（吹水用）

| 方向 | 方案 |
|------|------|
| 文件传输 | 分片上传 + 进度回调，每种消息 type 对应一种处理分支 |
| 群聊 | 频道概念 + 消息广播，服务端维护频道成员表 |
| 加密 | QSslSocket 替换 QTcpSocket（TLS 证书），或应用层 AES 加密 |
| 分布式 | Redis Pub/Sub 做跨服务器消息路由，多实例水平扩展 |
| 离线消息 | 服务端缓存未送达消息，客户端重连后拉取 |
| 已读/未读 | 消息表加 read 字段，客户端 ACK 机制 |
| WebSocket | 改协议适配 Web 端，QWebSocket 替换 QTcpSocket |
