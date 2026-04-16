# qREST数据传输协议规范

**文档版本**: v1.0.0\
**最后更新**: 2026-04-07\
**文档状态**: 正式发布\
**文档编号**: QREST-PROTOCOL-001

***

## 文档信息

| 项目   | 内容            |
| ---- | ------------- |
| 文档名称 | qREST数据传输协议规范 |
| 版本号  | v1.0.0        |
| 创建日期 | 2026-01-06    |
| 最后更新 | 2026-04-07    |
| 文档状态 | 正式发布          |
| 编写人员 | qREST开发团队     |
| 审核人员 | 技术架构组         |
| 批准人员 | 项目负责人         |

***

**前置知识要求**:

- 了解qREST文件存储格式规范
- 熟悉HTTP/HTTPS协议
- 了解RESTful API设计原则
- 掌握JSON数据格式
- 具备基本的编程能力
- 了解建筑结构监测基础知识

***

## 文档约定

### 符号说明

| 符号 | 说明      |
| -- | ------- |
| ✓  | 支持/推荐   |
| ✗  | 不支持/禁止  |
| ⚠  | 警告/注意事项 |
| 💡 | 提示/建议   |
| 🔒 | 安全相关    |
| ⚡  | 性能相关    |

***

## 目录

1. [传输协议概述](#1-传输协议概述-1)
2. [协议架构](#2-协议架构-1)
3. [认证与授权](#3-认证与授权-1)
4. [数据校验规则](#4-数据校验规则-1)
5. [建筑结构轻量化地震监测数据获取](#5-建筑结构轻量化地震监测数据获取-1)
6. [REST API接口定义](#6-rest-api接口定义-1)
7. [错误处理机制](#7-错误处理机制-1)

***

## 1. 传输协议概述

### 1.1 协议目标

qREST数据传输协议旨在为建筑结构轻量化地震监测数据提供一套 **完整、高效、可靠** 的数据传输规范。

**核心目标**：

| 目标       | 说明                             | 优先级  |
| -------- | ------------------------------ | ---- |
| **统一性**  | 提供统一的API接口规范，降低前后端开发复杂度        | 🔒 高 |
| **高效性**  | 支持REST API和二进制流两种传输方式，满足不同场景需求 | 🔒 高 |
| **可靠性**  | 完善的数据校验和错误处理机制，确保数据传输的准确性      | 🔒 高 |
| **安全性**  | 基于JWT的认证授权机制，保障数据访问安全          | 🔒 高 |
| **可扩展性** | 模块化设计，支持新功能的快速接入               | ⚡ 中  |

### 1.2 适用场景

本协议适用于以下场景：

| 场景      | 传输方式                     | 说明              |
| ------- | ------------------------ | --------------- |
| 前后端数据交互 | REST API (JSON)          | 用户管理、数据源配置、查询等  |
| 实时数据流传输 | Binary Stream            | 建筑结构轻量化地震监测数据传输 |
| 第三方系统集成 | REST API + Binary Stream | 外部系统数据访问        |

### 1.3 协议特点

| 特点     | 说明                    |
| ------ | --------------------- |
| 双模式传输  | 支持REST API和二进制流两种传输模式 |
| 标准化接口  | 遵循RESTful设计原则         |
| 强类型校验  | 完善的数据校验机制             |
| 统一错误处理 | 标准化的错误响应格式            |
| 版本控制   | 支持多版本并存和平滑升级          |

***

## 2. 协议架构

### 2.1 整体架构

```mermaid
graph TB
    subgraph Client["客户端层"]
        WEB["Web浏览器"]
        MOBILE["移动应用"]
        THIRD["第三方系统"]
    end

    subgraph Transport["传输协议层"]
        REST["REST API (JSON)<br/>- 用户管理<br/>- 数据源管理<br/>- 配置管理"]
        BINARY["Binary Stream (二进制)<br/>- 实时数据流<br/>- 高效传输<br/>- 低延迟"]
    end

    subgraph Service["服务层"]
        AUTH["认证服务"]
        DATA["数据服务"]
        FILE["文件服务"]
    end

    subgraph Data["数据层"]
        DB["数据库"]
        FS["文件系统"]
        CACHE["缓存系统"]
    end

    WEB & MOBILE & THIRD --> Transport
    REST & BINARY --> Service
    Service --> Data

    style Client fill:#1565c0,stroke:#0d47a1,color:#fff
    style Transport fill:#1976d2,stroke:#1565c0,color:#fff
    style Service fill:#1e88e5,stroke:#1565c0,color:#fff
    style Data fill:#2196f3,stroke:#1565c0,color:#fff
```

## 3. 认证与授权

### 3.1 认证机制

本协议采用JWT（JSON Web Token）进行身份认证。

#### 3.1.1 JWT Token结构

JWT Token由三部分组成：

```
Header.Payload.Signature
```

**Header示例**:

```json
{
  "alg": "HS256",
  "typ": "JWT"
}
```

**Payload示例**:

```json
{
  "user_id": 1,
  "username": "admin",
  "role": "admin",
  "exp": 1712345678,
  "iat": 1712342078
}
```

#### 3.1.2 Token使用方式

客户端在请求头中携带Token：

```http
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

### 3.2 授权机制

#### 3.2.1 角色权限

| 角色    | 权限说明           |
| ----- | -------------- |
| admin | 系统管理员，拥有所有权限   |
| user  | 普通用户，可访问授权的数据源 |

#### 3.2.2 权限验证流程

```mermaid
flowchart LR
    A["请求"] --> B["Token验证"]
    B --> C["角色检查"]
    C --> D["权限验证"]
    D --> E["执行操作"]

    A -.->|携带Token| B
    B -.->|解析Token| C
    C -.->|获取角色| D
    D -.->|检查权限| E
    E -.->|返回结果| A

    style A fill:#ff9800,stroke:#f57c00,color:#fff
    style E fill:#4caf50,stroke:#388e3c,color:#fff
```

### 3.3 登录流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Server as 服务端

    Client->>Server: 1. POST /api/auth/login<br/>{username, password}
    Server->>Server: 2. 验证用户名密码
    Server->>Server: 3. 生成JWT Token
    Server-->>Client: 4. 返回Token和用户信息
    Client->>Client: 5. 保存Token
```

***

## 4. 数据校验规则

### 4.1 校验机制

#### 4.1.1 JSON数据校验

**字段类型校验**:

```json
{
  "name": "string",      // 必须为字符串
  "age": 25,             // 必须为整数
  "price": 99.99,        // 必须为浮点数
  "enabled": true        // 必须为布尔值
}
```

**字段约束校验**:

```json
{
  "username": {
    "type": "string",
    "minLength": 3,
    "maxLength": 20,
    "pattern": "^[a-zA-Z0-9_]+$"
  },
  "age": {
    "type": "integer",
    "minimum": 0,
    "maximum": 150
  }
}
```

#### 4.1.2 二进制数据校验

**CRC32校验**:

```mermaid
flowchart LR
    Data["数据内容"] --> CRC["计算CRC32校验和"]
    CRC --> Compare["与包头校验和比对"]
    Compare -->|一致| Valid["数据完整"]
    Compare -->|不一致| Invalid["丢弃数据包"]

    style Data fill:#1976d2,stroke:#1565c0,color:#fff
    style Valid fill:#4caf50,stroke:#388e3c,color:#fff
    style Invalid fill:#f44336,stroke:#d32f2f,color:#fff
```

校验流程：

1. 计算数据内容的CRC32校验和
2. 与包头中的校验和比对
3. 一致则数据完整，否则丢弃

### 4.2 校验错误处理

| 错误类型    | 错误码  | 处理方式        |
| ------- | ---- | ----------- |
| 字段缺失    | 2001 | 返回缺失字段列表    |
| 类型错误    | 2002 | 返回期望类型和实际类型 |
| 值域错误    | 2003 | 返回允许范围      |
| CRC校验失败 | 2004 | 丢弃数据包       |

***

## 5. 建筑结构轻量化地震监测数据获取

### 5.1 数据包结构

二进制数据包采用小端序（Little-Endian）存储，包含包头和数据体两部分：

```mermaid
flowchart LR
    subgraph Packet["数据包结构"]
        Header["包头 (Header)<br/>固定32字节"]
        Body["数据体 (Body)<br/>N字节"]
    end

    Header --> Body

    style Header fill:#1565c0,stroke:#0d47a1,color:#fff
    style Body fill:#1976d2,stroke:#1565c0,color:#fff
```

### 5.2 包头结构

包头固定32字节，包含以下字段：

| 偏移量 | 字段名            | 类型     | 大小  | 说明                    |
| --- | -------------- | ------ | --- | --------------------- |
| 0   | Magic          | uint16 | 2字节 | 包头标识，固定为0x7144 ("qD") |
| 2   | SourceID       | uint16 | 2字节 | 数据源ID                 |
| 4   | Version        | uint8  | 1字节 | 协议版本号，当前为0x01         |
| 5   | PacketType     | uint8  | 1字节 | 数据包类型                 |
| 6   | ChannelCount   | uint16 | 2字节 | 通道数量                  |
| 8   | DataEncodings  | uint16 | 2字节 | 数据编码方式                |
| 10  | SamplingRate   | uint16 | 2字节 | 采样率(Hz)               |
| 12  | DataPointCount | uint32 | 4字节 | 每个通道的数据点数量            |
| 16  | Timestamp      | uint64 | 8字节 | 时间戳（毫秒）               |
| 24  | BodySize       | uint32 | 4字节 | 数据体长度                 |
| 28  | Checksum       | uint32 | 4字节 | 数据体CRC32校验和           |

**结构体定义 (C 语言风格)**:

```c
typedef struct {
    uint16_t magic;           // 0x7144 ("qD")
    uint16_t source_id;      // 数据源ID
    uint8_t  version;         // 协议版本 0x01
    uint8_t  packet_type;     // 数据包类型
    uint16_t channel_count;   // 通道数量
    uint16_t data_encodings;  // 数据编码方式
    uint16_t sampling_rate;   // 采样率(Hz)
    uint32_t data_point_count;// 每个通道的数据点数量
    uint64_t timestamp;       // 时间戳（毫秒）
    uint32_t body_size;       // 数据体长度
    uint32_t checksum;        // CRC32校验和
} QRestPacketHeader;
```

### 5.3 数据体结构

数据体包含实际的监测数据，按通道顺序排列：

```mermaid
flowchart LR
    subgraph DataBody["数据体结构"]
        C1["通道1数据<br/>NPTS×TypeSize"]
        C2["通道2数据<br/>NPTS×TypeSize"]
        CN["通道N数据<br/>NPTS×TypeSize"]
        Dots[...]
    end

    C1 --> C2 --> CN --> Dots

    style C1 fill:#1565c0,stroke:#0d47a1,color:#fff
    style C2 fill:#1976d2,stroke:#1565c0,color:#fff
    style CN fill:#1e88e5,stroke:#1565c0,color:#fff
```

### 5.4 数据包类型定义

| PacketType值 | 类型名称   | 说明                |
| ----------- | ------ | ----------------- |
| 0x01        | 正常数据包  | 包含实际监测数据          |
| 0x02        | 心跳包    | 如没有数据，发送空包，保持连接活跃 |
| 0x03        | 错误/状态包 | 服务器错误，无法继续提供数据服务  |
| 0x04        | 结束包    | 数据流结束、停用数据源       |

### 5.5 数据编码方式定义

DataEncodings 字段定义了数据体中时序数据的编码方式：

| DataEncodings 值 | 编码方式                 | 说明                                  |
| --------------- | -------------------- | ----------------------------------- |
| 0               | 32-bit 浮点数 (Float32) | 每个数据点使用 4 字节表示，适用于大多数监测数据           |
| 1               | 64-bit 浮点数 (Float64) | 每个数据点使用 8 字节表示，适用于需要高精度的场景          |
| 10              | 16-bit 整数 (Int16)    | 每个数据点使用 2 字节表示，适用于资源受限的场景           |
| 11              | 32-bit 整数 (Int32)    | 每个数据点使用 4 字节表示，适用于需要较高精度但受限于存储空间的场景 |

### 5.6 数据体长度计算

数据体长度（BodySize）计算公式：

```
BodySize = ChannelCount × DataPointCount × TypeSize
```

其中 TypeSize 由 DataEncodings 字段指定（Float32=4, Float64=8, Int16=2, Int32=4）。

### 5.7 CRC32校验

Checksum 字段使用 CRC32 算法计算数据体部分的校验和。

**算法参数**：

| 参数         | 值          |
| ---------- | ---------- |
| Polynomial | 0x04C11DB7 |
| Init       | 0xFFFFFFFF |
| RefIn      | False      |
| RefOut     | False      |
| XorOut     | 0xFFFFFFFF |

**算法代码实现**：

```c
uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}
```

***

## 6. REST API接口定义

### 6.1 接口规范

#### 6.1.1 基础URL

```
https://api.example.com/api
```

#### 6.1.2 请求格式

**请求头**:

```http
Content-Type: application/json
Authorization: Bearer {token}
```

**请求体**:

```json
{
  "field1": "value1",
  "field2": "value2"
}
```

#### 6.1.3 响应格式

**成功响应**:

```json
{
  "data": {...},
  "message": "操作成功"
}
```

**错误响应**:

```json
{
  "code": 1001,
  "message": "用户名或密码错误",
  "details": [...]
}
```

### 6.2 认证接口

#### 6.2.1 用户登录

**请求**:

```http
POST /api/auth/login
Content-Type: application/json

{
  "username": "admin",
  "password": "admin123"
}
```

**响应**:

```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "user": {
    "id": 1,
    "username": "admin",
    "name": "管理员",
    "role": "admin"
  }
}
```

#### 6.2.2 获取当前用户

**请求**:

```http
GET /api/auth/me
Authorization: Bearer {token}
```

**响应**:

```json
{
  "id": 1,
  "username": "admin",
  "name": "管理员",
  "role": "admin",
  "status": "active",
  "created_at": "2026-04-01T12:00:00Z"
}
```

### 6.3 数据源管理接口

#### 6.3.1 获取数据源列表

**端点**：`GET /api/datasources`

**请求参数**：

| 参数名        | 类型      | 必需 | 位置    | 说明                    | 示例      |
| ---------- | ------- | -- | ----- | --------------------- | ------- |
| current    | integer | 否  | query | 当前页码，从1开始             | 1       |
| page\_size | integer | 否  | query | 每页数量，默认10             | 10      |
| status     | string  | 否  | query | 筛选状态：enabled/disabled | enabled |

**请求示例**：

```http
GET /api/datasources?current=1&page_size=10&status=enabled
Authorization: Bearer {token}
```

**返回参数**：

| 参数名                 | 类型      | 说明                                |
| ------------------- | ------- | --------------------------------- |
| data                | array   | 数据源数组                             |
| data\[].id          | integer | 数据源唯一标识                           |
| data\[].name        | string  | 数据源名称                             |
| data\[].description | string  | 数据源描述                             |
| data\[].type\_      | string  | 数据源类型：file/seedlink/jopens/sensor |
| data\[].status      | string  | 状态：enabled/disabled               |
| data\[].Metadata    | object  | 元数据信息，详见Metadata定义                |
| data\[].created\_at | string  | 创建时间，ISO 8601格式                   |
| data\[].updated\_at | string  | 更新时间，ISO 8601格式                   |
| total               | integer | 总数据条数                             |
| current             | integer | 当前页码                              |
| page\_size          | integer | 每页数量                              |

**响应示例**：

```json
{
  "data": [
    {
      "id": 1,
      "name": "测试数据源",
      "description": "测试数据源描述",
      "type_": "file",
      "status": "enabled",
      "Metadata": {...},
      "created_at": "2026-04-01T12:00:00Z",
      "updated_at": "2026-04-01T12:00:00Z"
    }
  ],
  "total": 1,
  "current": 1,
  "page_size": 10
}
```

**错误响应**：

| 错误码  | 错误信息 | 说明    |
| ---- | ---- | ----- |
| 1004 | 权限不足 | 无访问权限 |

#### 6.3.2 创建数据源

**端点**：`POST /api/datasources`

**请求参数**：

| 参数名         | 类型     | 必需 | 说明    | 约束                                |
| ----------- | ------ | -- | ----- | --------------------------------- |
| name        | string | ✓  | 数据源名称 | 长度1-100字符                         |
| description | string | 否  | 数据源描述 | 最大500字符                           |
| type\_      | string | ✓  | 数据源类型 | file/seedlink/jopens/sensor       |
| config      | object | ✓  | 数据源配置 | 根据type不同而不同                       |
| Metadata    | object | ✓  | 元数据信息 | 详见qREST文件存储格式规范V1.0.0中的Metadata定义 |

**config参数说明**：

| 参数名            | 类型      | 必需                      | 说明             |
| -------------- | ------- | ----------------------- | -------------- |
| file\_path     | string  | type=file时必需            | 数据文件路径         |
| sampling\_rate | integer | type=file时必需            | 采样率(Hz)        |
| loop           | boolean | 否                       | 是否循环播放，默认false |
| address        | string  | type=seedlink/jopens时必需 | 服务器地址          |
| port           | integer | type=seedlink/jopens时必需 | 服务器端口          |

**请求示例**：

```http
POST /api/datasources
Authorization: Bearer {token}
Content-Type: application/json

{
  "name": "测试数据源",
  "description": "测试数据源描述",
  "type_": "file",
  "config": {
    "file_path": "/data/test.qrest",
    "sampling_rate": 100,
    "loop": true
  },
  "Metadata": {...}
}
```

**返回参数**：

| 参数名         | 类型      | 说明                                      |
| ----------- | ------- | --------------------------------------- |
| id          | integer | 数据源唯一标识                                 |
| name        | string  | 数据源名称                                   |
| description | string  | 数据源描述                                   |
| type\_      | string  | 数据源类型                                   |
| status      | string  | 状态：enabled/disabled                     |
| config      | object  | 数据源配置                                   |
| Metadata    | object  | 元数据信息，详见qREST文件存储格式规范V1.0.0中的Metadata定义 |
| created\_at | string  | 创建时间，ISO 8601格式                         |
| updated\_at | string  | 更新时间，ISO 8601格式                         |

**响应示例**：

```json
{
  "id": 1,
  "name": "测试数据源",
  "description": "测试数据源描述",
  "type_": "file",
  "status": "disabled",
  "config": {
    "file_path": "/data/test.qrest",
    "sampling_rate": 100,
    "loop": true
  },
  "Metadata": {...},
  "created_at": "2026-04-01T12:00:00Z",
  "updated_at": "2026-04-01T12:00:00Z"
}
```

**错误响应**：

| 错误码  | 错误信息   | 说明              |
| ---- | ------ | --------------- |
| 2001 | 参数缺失   | 必需参数未提供         |
| 2002 | 参数格式错误 | 参数类型或格式不正确      |
| 2003 | 参数值域错误 | 参数值超出允许范围       |
| 1004 | 权限不足   | 无访问权限，需要admin角色 |

#### 6.3.3 获取单个数据源

**端点**：`GET /api/datasources/{id}`

**路径参数**：

| 参数名 | 类型      | 必需 | 说明      | 示例 |
| --- | ------- | -- | ------- | -- |
| id  | integer | ✓  | 数据源唯一标识 | 1  |

**请求示例**：

```http
GET /api/datasources/1
Authorization: Bearer {token}
```

**返回参数**：

| 参数名         | 类型      | 说明                                      |
| ----------- | ------- | --------------------------------------- |
| id          | integer | 数据源唯一标识                                 |
| name        | string  | 数据源名称                                   |
| description | string  | 数据源描述                                   |
| type\_      | string  | 数据源类型                                   |
| status      | string  | 状态：enabled/disabled                     |
| config      | object  | 数据源配置                                   |
| Metadata    | object  | 元数据信息，详见qREST文件存储格式规范V1.0.0中的Metadata定义 |
| created\_at | string  | 创建时间，ISO 8601格式                         |
| updated\_at | string  | 更新时间，ISO 8601格式                         |

**响应示例**：

```json
{
  "id": 1,
  "name": "测试数据源",
  "description": "测试数据源描述",
  "type_": "file",
  "status": "enabled",
  "config": {
    "file_path": "/data/test.qrest",
    "sampling_rate": 100,
    "loop": true
  },
  "Metadata": {...},
  "created_at": "2026-04-01T12:00:00Z",
  "updated_at": "2026-04-01T12:00:00Z"
}
```

**错误响应**：

| 错误码  | 错误信息  | 说明       |
| ---- | ----- | -------- |
| 3001 | 资源不存在 | 数据源ID不存在 |

#### 6.3.4 更新数据源

**端点**：`PUT /api/datasources/{id}`

**路径参数**：

| 参数名 | 类型      | 必需 | 说明      | 示例 |
| --- | ------- | -- | ------- | -- |
| id  | integer | ✓  | 数据源唯一标识 | 1  |

**请求参数**：

| 参数名         | 类型     | 必需 | 说明    | 约束           |
| ----------- | ------ | -- | ----- | ------------ |
| name        | string | 否  | 数据源名称 | 长度1-100字符    |
| description | string | 否  | 数据源描述 | 最大500字符      |
| config      | object | 否  | 数据源配置 | 根据type不同而不同  |
| Metadata    | object | 否  | 元数据信息 | 详见Metadata定义 |

**请求示例**：

```http
PUT /api/datasources/1
Authorization: Bearer {token}
Content-Type: application/json

{
  "name": "更新后的数据源名称",
  "description": "更新后的描述",
  "config": {
    "file_path": "/data/updated.qrest",
    "sampling_rate": 200,
    "loop": false
  },
  "Metadata": {...}
}
```

**返回参数**：

| 参数名         | 类型      | 说明                                      |
| ----------- | ------- | --------------------------------------- |
| id          | integer | 数据源唯一标识                                 |
| name        | string  | 数据源名称                                   |
| description | string  | 数据源描述                                   |
| type\_      | string  | 数据源类型                                   |
| status      | string  | 状态：enabled/disabled                     |
| config      | object  | 数据源配置                                   |
| Metadata    | object  | 元数据信息，详见qREST文件存储格式规范V1.0.0中的Metadata定义 |
| created\_at | string  | 创建时间，ISO 8601格式                         |
| updated\_at | string  | 更新时间，ISO 8601格式                         |

**响应示例**：

```json
{
  "id": 1,
  "name": "更新后的数据源名称",
  "description": "更新后的描述",
  "type_": "file",
  "status": "enabled",
  "config": {
    "file_path": "/data/updated.qrest",
    "sampling_rate": 200,
    "loop": false
  },
  "Metadata": {...},
  "created_at": "2026-04-01T12:00:00Z",
  "updated_at": "2026-04-01T12:30:00Z"
}
```

**错误响应**：

| 错误码  | 错误信息   | 说明              |
| ---- | ------ | --------------- |
| 2001 | 参数缺失   | 必需参数未提供         |
| 2002 | 参数格式错误 | 参数类型或格式不正确      |
| 2003 | 参数值域错误 | 参数值超出允许范围       |
| 3001 | 资源不存在  | 数据源ID不存在        |
| 1004 | 权限不足   | 无访问权限，需要admin角色 |

#### 6.3.5 删除数据源

**端点**：`DELETE /api/datasources/{id}`

**功能说明**：删除指定的数据源。删除操作会同时移除所有关联的配置和元数据。删除成功后返回204状态码。

**请求参数**：

| 参数名 | 类型      | 位置   | 必需 | 说明      | 约束           |
| --- | ------- | ---- | -- | ------- | ------------ |
| id  | integer | path | ✓  | 数据源唯一标识 | 必须是已存在的数据源ID |

**返回参数**：

删除成功返回 HTTP 204 No Content，无响应内容。

**请求示例**:

```http
DELETE /api/datasources/1
Authorization: Bearer {token}
```

**响应**:

```http
HTTP/1.1 204 No Content
```

**错误响应**：

| 错误码  | 错误信息  | 说明       |
| ---- | ----- | -------- |
| 3001 | 资源不存在 | 数据源ID不存在 |
| 3002 | 资源已删除 | 数据源已被删除  |

#### 6.3.6 启用数据源

**端点**：`POST /api/datasources/{id}/enable`

**功能说明**：启用指定的数据源。启用后数据源可以正常提供数据服务。

**请求参数**：

| 参数名 | 类型      | 位置   | 必需 | 说明      | 约束                    |
| --- | ------- | ---- | -- | ------- | --------------------- |
| id  | integer | path | ✓  | 数据源唯一标识 | 必须是已存在且当前处于禁用状态的数据源ID |

**返回参数**：

| 参数名     | 类型      | 说明                  |
| ------- | ------- | ------------------- |
| success | boolean | 操作是否成功              |
| id      | integer | 数据源唯一标识             |
| status  | string  | 更新后的状态，值为 "enabled" |

**请求示例**:

```http
POST /api/datasources/1/enable
Authorization: Bearer {token}
```

**响应**:

```json
{
  "success": true,
  "id": 1,
  "status": "enabled"
}
```

**错误响应**：

| 错误码  | 错误信息  | 说明         |
| ---- | ----- | ---------- |
| 3001 | 资源不存在 | 数据源ID不存在   |
| 3003 | 状态冲突  | 数据源已处于启用状态 |

#### 6.3.7 禁用数据源

**端点**：`POST /api/datasources/{id}/disable`

**功能说明**：禁用指定的数据源。禁用后数据源将暂停提供数据服务，但不会删除数据。

**请求参数**：

| 参数名 | 类型      | 位置   | 必需 | 说明      | 约束                    |
| --- | ------- | ---- | -- | ------- | --------------------- |
| id  | integer | path | ✓  | 数据源唯一标识 | 必须是已存在且当前处于启用状态的数据源ID |

**返回参数**：

| 参数名     | 类型      | 说明                   |
| ------- | ------- | -------------------- |
| success | boolean | 操作是否成功               |
| id      | integer | 数据源唯一标识              |
| status  | string  | 更新后的状态，值为 "disabled" |

**请求示例**:

```http
POST /api/datasources/1/disable
Authorization: Bearer {token}
```

**响应**:

```json
{
  "success": true,
  "id": 1,
  "status": "disabled"
}
```

**错误响应**：

| 错误码  | 错误信息  | 说明         |
| ---- | ----- | ---------- |
| 3001 | 资源不存在 | 数据源ID不存在   |
| 3003 | 状态冲突  | 数据源已处于禁用状态 |

### 6.4 数据服务接口

#### 6.4.1 获取活跃数据源

**请求**:

```http
GET /api/data-service/active-datasource
Authorization: Bearer {token}
```

**响应**:

```json
{
  "id": 1,
  "name": "测试数据源",
  "type_": "file",
  "status": "enabled",
  "Metadata": {...},
  "last_updated": "2026-04-01T12:00:00Z"
}
```

#### 6.4.2 数据流订阅

数据流订阅使用二进制数据流方式传输实时监测数据。

**请求**:

```http
GET /api/data-service/stream?datasource_id=1
Authorization: Bearer {token}
Accept: application/octet-stream
```

**响应**:

二进制数据流，每个数据包包含32字节包头 + N字节数据体，详见5.1-5.7节。
**传输特性**:

| 特性   | 说明                              |
| ---- | ------------------------------- |
| 传输格式 | 二进制流 (application/octet-stream) |
| 数据封装 | 32字节包头 + 数据体                    |
| 连接类型 | 长连接                             |
| 心跳机制 | PacketType=0x02 心跳包保持连接活跃       |

***

## 7. 错误处理机制

### 7.1 错误响应格式

所有错误响应采用统一的JSON格式：

```json
{
  "code": 1001,
  "message": "用户名或密码错误",
  "details": [
    {
      "field": "username",
      "message": "用户名不能为空",
      "value": null
    }
  ],
  "request_id": "req_123456",
  "timestamp": "2026-04-01T12:00:00Z"
}
```

### 7.2 错误码分类

| 错误码范围     | 分类   | 说明           |
| --------- | ---- | ------------ |
| 1000-1999 | 认证错误 | Token相关、权限相关 |
| 2000-2999 | 参数错误 | 参数缺失、格式错误    |
| 3000-3999 | 资源错误 | 资源不存在、已删除    |
| 4000-4999 | 业务错误 | 数据源错误、数据处理错误 |
| 5000-5999 | 系统错误 | 服务器内部错误      |

### 7.3 常见错误码

| 错误码  | 错误信息     | 说明         |
| ---- | -------- | ---------- |
| 1001 | 用户名或密码错误 | 登录失败       |
| 1002 | Token已过期 | 需要刷新Token  |
| 1003 | Token无效  | 需要重新登录     |
| 1004 | 权限不足     | 无访问权限      |
| 2001 | 参数缺失     | 必需参数未提供    |
| 2002 | 参数格式错误   | 参数类型或格式不正确 |
| 3001 | 资源不存在    | 请求的资源不存在   |
| 4001 | 数据源未启用   | 数据源未激活     |
| 5004 | 服务器内部错误  | 系统异常       |

***

**文档结束**
