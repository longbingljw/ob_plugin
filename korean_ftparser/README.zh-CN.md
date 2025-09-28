# OceanBase 韩语全文解析插件（Korean Fulltext Parser Plugin）

面向 OceanBase 的韩语全文解析插件。核心使用 JNI 桥接 Java 分词库（已集成 Apache Lucene KoreanAnalyzer/Nori）。

## 🎯 功能特性

- ✅ 兼容 OceanBase FTParser 接口（`ftparser_main.cpp`）
- ✅ JNI 集成调用 Java 韩语分词（Lucene KoreanAnalyzer/Nori）
- ✅ UTF-8 多字节韩文处理
- ✅ 并发安全：单 JVM 复用 + 线程独立 `JNIEnv*`（`thread_local`）
- ✅ 局部引用安全：`PushLocalFrame/PopLocalFrame` 管理 JNI 局部引用
- ✅ 可扩展：可替换 Java 分词实现

## 📁 项目结构

```
korean_ftparser
├── README.zh-CN.md
├── build/
│   └── libkorean_ftparser.so      # 插件 .so
├── java/
│   ├── KoreanSegmenter.java       # Nori 驱动的 Java 韩语分词器
│   ├── KoreanSegmenter.class
│   └── lib/
│       ├── lucene-core-8.11.2.jar
│       ├── lucene-analyzers-common-8.11.2.jar
│       └── lucene-analyzers-nori-8.11.2.jar
```

## 🚀 快速开始

### 一键部署（推荐方式）

**最简单的方法**：直接将整个 `java/` 文件夹复制到 Observer 工作目录

```bash
# 1. 复制插件动态库
cp /path/to/yourplugindirpath/libkorean_ftparser.so /path/to/observer/plugin_dir/

# 2. 复制整个 java 目录（无需其他的目录配置）
cp -r java/ /path/to/observer/java/

# 3. 安装所需依赖
yum install java-1.8.0-openjdk-devel -y
yum install libaio -y

# 4. 启动 Observer 并加载插件
# 连接数据库
obclient -h127.0.0.1 -P2881 -uroot@sys -pdifyai123456 # 这里以dify的数据库连接信息为例

# 在sys租户中设置插件加载
ALTER SYSTEM SET plugins_load='libkorean_ftparser.so:on';

# 重启Observer生效
killall observer
cd /path/to/observer
./bin/observer  # 在observer工作目录执行

# 验证安装（见下文）

# 5. 在Dify中的配置
# 如果需要在Docker启动的Dify中使用韩语分词器：
cd docker
bash setup-mysql-env.sh
# 修改.env中的配置：
# OCEANBASE_ENABLE_HYBRID_SEARCH=true
# OCEANBASE_FULLTEXT_PARSER=korean_ftparser
docker compose up -d
# 进入oceanbase容器：docker exec -it <container_name> bash
# 容器内observer工作目录为 /root/ob/observer
# 使用docker cp /path/to/korean-ftparser <container_name>:/path/to/korean-ftparser
# 按照上述步骤1-4配置插件
```


> 📖 **详细插件使用说明**：参考 [OceanBase Plugin Development Kit 用户手册](https://oceanbase.github.io/oceanbase-plugin-dev-kit/user-guide/)

### 依赖寻找优先级

插件按以下优先级自动寻找Java依赖：

1. **🥇 环境变量**（最高优先级）
   ```bash
   export KOREAN_PARSER_CLASSPATH="/custom/path/lucene-core-8.11.2.jar:/custom/path/lucene-analyzers-common-8.11.2.jar:/custom/path/lucene-analyzers-nori-8.11.2.jar:/custom/path"
   ```

2. **🥈 Observer 工作目录**（推荐）
   ```
   ${OB_WORKDIR}/java/lib/lucene-core-8.11.2.jar
   ${OB_WORKDIR}/java/lib/lucene-analyzers-common-8.11.2.jar
   ${OB_WORKDIR}/java/lib/lucene-analyzers-nori-8.11.2.jar
   ${OB_WORKDIR}/java/KoreanSegmenter.class
   ```

3. **🥉 插件相对路径**（开发环境）
   ```
   ../java/lib/lucene-*.jar
   ./java/lib/lucene-*.jar
   ../../java/lib/lucene-*.jar
   ```

**💡 建议**：使用方式2（复制java目录），无需配置KOREAN_PARSER_CLASSPATH，快速体验

### 验证安装

```sql
-- 检查插件是否加载成功
SELECT * FROM oceanbase.GV$OB_PLUGINS WHERE NAME = 'korean_ftparser';

-- 创建测试表（注意shell的字符集编码用UTF-8）
CREATE TABLE t_korean (
    c1 INT,
    c2 VARCHAR(200),
    c3 TEXT,
    FULLTEXT INDEX (c2, c3) WITH PARSER korean_ftparser
);

-- 插入韩语测试数据
INSERT INTO t_korean (c1, c2, c3) VALUES
(1, '안녕하세요', '안녕하세요, 우리 웹사이트에 오신 것을 환영합니다'),
(2, '감사합니다', '방문해 주셔서 감사합니다'),
(3, '문의하기', '질문이 있으시면 영업일에 언제든지 문의해 주세요'),
(4, '감사합니다', '서비스를 이용해 주셔서 감사합니다'),
(5, '환영합니다', 'OceanBase에 오신 것을 환영합니다'),
(6, '안녕하세요', '안녕하세요, 다시 만날 수 있기를 기대합니다'),
(7, '어떻게 지내세요', '요즘 어떻게 지내세요'),
(8, '문의하기', '질문이 있으시면 영업일에 언제든지 문의해 주세요'),
(9, '문제없습니다', '아무 문제 없습니다'),
(10, '양식 입력', '정보를 완전히 입력해 주세요'),
(11, '감사했습니다', '감사했습니다, 앞으로 다시 만날 수 있기를 바랍니다'),
(12, '안녕하세요', '안녕하세요, 안녕하세요'),
(13, '테스트 반복', '파서가 중복 단어를 적절히 처리하는지 테스트합니다'),
(14, '아무거나 좋아', '당신이 필요한 것은 무엇이든지'),
(15, '아무도 이해하지 못해', '무슨 일이 일어나고 있는지 아무도 이해하지 못합니다'),
(16, '평소대로', '모든 것이 평소대로입니다'),
(17, '2025년은 좋은 해', '2025년은 개발에 좋은 해입니다'),
(18, '2025년 감사', '2025년 감사합니다'),
(19, 'OceanBase 데이터베이스', 'OceanBase 데이터베이스를 선택하는 이유');
-- 测试全文搜索功能
-- 测试 1：匹配单个词（预计返回 c1 = 3, 8）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('문의' IN NATURAL LANGUAGE MODE);

-- 测试 2：匹配多个词（预计返回 c1 = 1, 5, 6, 12）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('안녕하세요 환영합니다' IN NATURAL LANGUAGE MODE);

-- 测试 3：停用词测试（如果 "의" 是停用词，应该无结果）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('의' IN NATURAL LANGUAGE MODE);

-- 测试 4：数字 + 韩语混合（预计返回 c1 = 17, 18）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('2025' IN NATURAL LANGUAGE MODE);

-- 测试 5：重复词语（预计返回 c1 = 13）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('테스트' IN NATURAL LANGUAGE MODE);

-- 测试 6：模糊匹配（预计返回 c1 = 2, 4, 11, 18）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('감사' IN NATURAL LANGUAGE MODE);

-- 测试 7：验证关键韩语句子搜索（预计返回 c1 = 19）
SELECT * FROM t_korean
WHERE MATCH(c2, c3) AGAINST('데이터베이스' IN NATURAL LANGUAGE MODE);
```

## 🏗️ 架构

### 四层设计

1. 插件接口层
   - 实现 OceanBase FTParser 回调
   - 插件生命周期与注册
   - C 接口

2. 解析核心层
   - 解析会话与 token 缓存
   - UTF-8 韩文处理
   - 集成 JNI 桥

3. JNI 桥接层
   - 单 JVM 复用（集中管理）
   - 每线程 `JNIEnv*` 自动获取与分离
   - `PushLocalFrame/PopLocalFrame` 管理局部引用
   - 异常捕获与错误码回传

4. Java 分词层
   - 具体韩语分词实现（Nori）

