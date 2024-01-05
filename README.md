# 基于流式套接字的网络聊天程序

## 环境

1. 编译器：gcc version 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04)
2. 数据库：mysql  Ver 8.0.35-0ubuntu0.22.04.1 for Linux on x86_64 ((Ubuntu))

## 编译方法

`makefile`文件所在目录，使用命令`make`即可编译得到文件

## 特性

**服务器支持多线程**

## 数据库设计命令

### 创建basic表

表中含有userid、username、password、online字段。

**userid为主键。**

userid、username、password字段都是varchar 8位类型，online为bool类型

```sql
CREATE TABLE basic (
    userid VARCHAR(8) PRIMARY KEY,
    username VARCHAR(8),
    password VARCHAR(8),
    online BOOLEAN
);
```

### 创建friends表(好友关系表)

表中含有serial、friend_id_1、friend_id_2字段。

**serial为主键，friend_id_1、friend_id_2为外键。**

serial是int类型，friend_id_1、friend_id_2字段是varchar 8位类型。

```sql
CREATE TABLE friends (
    serial INT AUTO_INCREMENT PRIMARY KEY,
    friend_id_1 VARCHAR(8),
    friend_id_2 VARCHAR(8),
    FOREIGN KEY (friend_id_1) REFERENCES basic(userid),
    FOREIGN KEY (friend_id_2) REFERENCES basic(userid)
);
```