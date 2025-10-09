# 实验插件部署指南

## 📦 部署文件清单

实验插件需要部署以下文件：

```
1. liboceanbase_jni_common.so          - 公共JNI管理库（61KB）
2. libexperimental_japanese_ftparser.so - 日语分词插件（2.3MB）
3. java/                                - Java分词类和依赖库
   ├── JapaneseSegmenter.class
   └── lib/
       ├── lucene-core-8.11.2.jar
       ├── lucene-analyzers-common-8.11.2.jar
       └── lucene-analyzers-kuromoji-8.11.2.jar
```

## 🔧 部署方案（3种方式）

### 方案1：标准部署到Observer插件目录（推荐）

```bash
# 1. 复制公共库到Observer的plugin_dir
sudo cp common/liboceanbase_jni_common/build/liboceanbase_jni_common.so \
        /path/to/observer/plugin_dir/

# 2. 复制插件库到同一目录
sudo cp experimental_japanese_ftparser/build/libexperimental_japanese_ftparser.so \
        /path/to/observer/plugin_dir/

# 3. 复制Java文件到Observer工作目录
sudo cp -r experimental_japanese_ftparser/java \
        /path/to/observer/

# 4. 在OceanBase中加载插件
ALTER SYSTEM SET plugins_load='libexperimental_japanese_ftparser.so:on';

✅ 优点：
- 所有.so文件在同一目录，自动找到
- 符合OceanBase插件管理习惯
- 无需修改系统配置

⚠️ 注意：
- 确保plugin_dir目录权限正确
- 公共库和插件库必须在同一目录
```

### 方案2：使用RPATH（适合开发测试）

修改CMakeLists.txt设置RPATH：

```cmake
# experimental_japanese_ftparser/CMakeLists.txt

# 设置RPATH，在运行时自动查找公共库
SET_TARGET_PROPERTIES(${PLUGIN_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../common/liboceanbase_jni_common/build"
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
)
```

部署步骤：
```bash
# 1. 重新编译插件（已包含RPATH）
cd experimental_japanese_ftparser/build
cmake .. && make

# 2. 创建lib目录
sudo mkdir -p /path/to/observer/plugin_dir/lib

# 3. 复制库文件
sudo cp ../../common/liboceanbase_jni_common/build/liboceanbase_jni_common.so \
        /path/to/observer/plugin_dir/lib/

sudo cp libexperimental_japanese_ftparser.so \
        /path/to/observer/plugin_dir/

# 4. 复制Java文件
sudo cp -r ../java /path/to/observer/

✅ 优点：
- 插件会自动在$ORIGIN和$ORIGIN/../lib中查找公共库
- 灵活的目录布局

⚠️ 注意：
- 需要重新编译
- 路径结构必须符合RPATH设置
```

### 方案3：使用LD_LIBRARY_PATH（适合临时测试）

```bash
# 1. 设置环境变量
export LD_LIBRARY_PATH=/path/to/common_lib:$LD_LIBRARY_PATH

# 2. 启动Observer
cd /path/to/observer
./bin/observer

✅ 优点：
- 快速测试
- 无需移动文件

⚠️ 注意：
- 环境变量重启后失效
- 不推荐生产环境使用
```

## 🎯 推荐的生产部署结构

```
/path/to/observer/
├── plugin_dir/
│   ├── liboceanbase_jni_common.so              ← 公共库
│   ├── libexperimental_japanese_ftparser.so    ← 日语插件
│   ├── libexperimental_korean_ftparser.so      ← 韩语插件（未来）
│   └── libexperimental_thai_ftparser.so        ← 泰语插件（未来）
└── java/
    ├── JapaneseSegmenter.class
    ├── KoreanSegmenter.class     （未来）
    ├── RealThaiSegmenter.class   （未来）
    └── lib/
        ├── lucene-core-8.11.2.jar
        ├── lucene-analyzers-common-8.11.2.jar
        ├── lucene-analyzers-kuromoji-8.11.2.jar
        └── ... (其他Lucene库)

✅ 所有插件共享：
  - 同一个liboceanbase_jni_common.so
  - 同一套Java库
  - 统一的JVM实例
```

## 🔍 验证部署

### 1. 检查库依赖
```bash
ldd /path/to/observer/plugin_dir/libexperimental_japanese_ftparser.so

# 应该看到：
# liboceanbase_jni_common.so => /path/to/observer/plugin_dir/liboceanbase_jni_common.so
# 如果显示"not found"，说明路径配置有问题
```

### 2. 检查插件加载
```sql
-- 在OceanBase中执行
SELECT * FROM oceanbase.GV$OB_PLUGINS 
WHERE NAME = 'experimental_japanese_ftparser';

-- 应该看到：
-- NAME: experimental_japanese_ftparser
-- STATUS: ACTIVE
```

### 3. 测试功能
```sql
-- 创建测试表
CREATE TABLE test_japanese (
    id INT,
    content TEXT,
    FULLTEXT INDEX (content) WITH PARSER experimental_japanese_ftparser
);

-- 插入测试数据
INSERT INTO test_japanese VALUES 
(1, 'こんにちは世界'),
(2, '日本語の自然言語処理'),
(3, 'OceanBaseデータベース');

-- 测试全文搜索
SELECT * FROM test_japanese 
WHERE MATCH(content) AGAINST('こんにちは' IN NATURAL LANGUAGE MODE);

-- 应该返回包含'こんにちは'的记录
```

## ⚙️ CMakeLists.txt优化建议

如果选择方案2（RPATH），修改`experimental_japanese_ftparser/CMakeLists.txt`：

```cmake
# 在文件末尾添加
# Set RPATH for runtime library search
SET_TARGET_PROPERTIES(${PLUGIN_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../common/liboceanbase_jni_common/build"
    INSTALL_RPATH "$ORIGIN"  # 在插件同目录查找
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)
```

## 🚨 常见问题排查

### 问题1：liboceanbase_jni_common.so: cannot open shared object file

**原因**：运行时找不到公共库

**解决**：
```bash
# 检查公共库是否存在
ls -l /path/to/observer/plugin_dir/liboceanbase_jni_common.so

# 检查权限
sudo chmod 755 /path/to/observer/plugin_dir/liboceanbase_jni_common.so

# 检查LD_LIBRARY_PATH
echo $LD_LIBRARY_PATH
```

### 问题2：JNI_CreateJavaVM failed

**原因**：找不到Java库或JVM

**解决**：
```bash
# 检查JAVA_HOME
echo $JAVA_HOME

# 检查JVM库
ls -l $JAVA_HOME/jre/lib/amd64/server/libjvm.so

# 添加到LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$JAVA_HOME/jre/lib/amd64/server:$LD_LIBRARY_PATH
```

### 问题3：ClassNotFoundException: JapaneseSegmenter

**原因**：找不到Java类

**解决**：
```bash
# 检查Java文件
ls -l /path/to/observer/java/JapaneseSegmenter.class

# 检查CLASSPATH环境变量（如果插件使用了）
# 或者确保java文件在Observer工作目录下
```

## 📝 完整部署脚本

创建自动部署脚本`deploy_experimental_plugin.sh`：

```bash
#!/bin/bash

# 配置
OBSERVER_DIR="/path/to/observer"
PLUGIN_DIR="$OBSERVER_DIR/plugin_dir"
WORKSPACE="/home/longbing.ljw/workspace/ob_plugin"

# 检查Observer目录
if [ ! -d "$OBSERVER_DIR" ]; then
    echo "Error: Observer directory not found: $OBSERVER_DIR"
    exit 1
fi

# 创建插件目录（如果不存在）
sudo mkdir -p "$PLUGIN_DIR"

# 部署公共库
echo "Deploying common library..."
sudo cp "$WORKSPACE/common/liboceanbase_jni_common/build/liboceanbase_jni_common.so" \
        "$PLUGIN_DIR/"

# 部署插件
echo "Deploying experimental Japanese FTParser..."
sudo cp "$WORKSPACE/experimental_japanese_ftparser/build/libexperimental_japanese_ftparser.so" \
        "$PLUGIN_DIR/"

# 部署Java文件
echo "Deploying Java files..."
sudo cp -r "$WORKSPACE/experimental_japanese_ftparser/java" \
        "$OBSERVER_DIR/"

# 设置权限
sudo chmod 755 "$PLUGIN_DIR"/*.so
sudo chmod -R 755 "$OBSERVER_DIR/java"

# 验证
echo ""
echo "Deployment completed!"
echo ""
echo "Files deployed:"
ls -lh "$PLUGIN_DIR"/liboceanbase_jni_common.so
ls -lh "$PLUGIN_DIR"/libexperimental_japanese_ftparser.so
echo ""
echo "Java files:"
ls -lh "$OBSERVER_DIR/java"

echo ""
echo "Next steps:"
echo "1. Start Observer"
echo "2. Run: ALTER SYSTEM SET plugins_load='libexperimental_japanese_ftparser.so:on';"
echo "3. Verify: SELECT * FROM oceanbase.GV\$OB_PLUGINS;"
```

使用方法：
```bash
chmod +x deploy_experimental_plugin.sh
./deploy_experimental_plugin.sh
```

## 🎯 总结

**推荐部署方式**：方案1（标准部署）
- 简单可靠
- 符合OceanBase插件管理习惯
- 所有.so文件放在plugin_dir同一目录

**关键点**：
1. ✅ `liboceanbase_jni_common.so`和`libexperimental_japanese_ftparser.so`必须在同一目录
2. ✅ Java文件必须在Observer能找到的路径（默认为工作目录下的java/）
3. ✅ 所有文件需要正确的权限（755）
4. ✅ JVM能正确初始化（JAVA_HOME设置正确）

部署成功后，所有使用公共库的插件都会自动协同工作！🎉
