# Thai Fulltext Parser Plugin for OceanBase

Thai fulltext parser plugin for OceanBase. It integrates a Java-based Thai segmentation library via JNI (Apache Lucene ThaiAnalyzer).

## 🎯 Features

- ✅ OceanBase FTParser interface compliant (`thai_ftparser_main.cpp`)
- ✅ JNI integration with Lucene ThaiAnalyzer
- ✅ Correct UTF-8 Thai handling
- ✅ Concurrency-safe: single JVM reuse + per-thread `JNIEnv*` (`thread_local`)
- ✅ Local reference safety via `PushLocalFrame/PopLocalFrame`
- ✅ Extensible Java segmenter implementation

## 📁 Project Structure

```
thai_ftparser_java/
├── CMakeLists.txt
├── README.md
├── README.zh-CN.md
├── oceanbase_local/                 # OceanBase headers for local build
├── build/                           # Build outputs (generated)
│   └── libthai_ftparser.so          # Plugin .so
├── java/
│   ├── RealThaiSegmenter.java       # Java segmenter powered by Lucene
│   ├── RealThaiSegmenter.class
│   └── lib/
│       ├── lucene-core-8.11.2.jar
│       └── lucene-analyzers-common-8.11.2.jar
├── thai_ftparser_main.cpp           # OceanBase plugin interface
├── thai_parser_core.h/.cpp          # Parser core
├── thai_jni_bridge.h/.cpp           # JVM/thread/exception/ref management
```

## 🚀 Quick Start

### Build environment (for compiling the plugin)

```bash
yum install -y cmake make glibc-devel glibc-headers gcc gcc++
yum install -y yum-utils
yum-config-manager --add-repo https://mirrors.aliyun.com/oceanbase/OceanBase.repo
yum install -y oceanbase-plugin-dev-kit
```

### Build

```bash
# 1) Compile Java segmenter (requires Lucene jars)
cd java
javac -cp "lib/lucene-core-8.11.2.jar:lib/lucene-analyzers-common-8.11.2.jar:." RealThaiSegmenter.java
cd ..

# 2) Build the plugin
mkdir -p build && cd build
cmake .. && make
cd ..
```

### Install to OceanBase

- Put `build/libthai_ftparser.so` into `plugin_dir/` of the Observer working directory
- Put `java/RealThaiSegmenter.class` into `${OB_WORKDIR}/java/`
- Put Lucene jars into `${OB_WORKDIR}/java/lib/`
- Or set `THAI_PARSER_CLASSPATH` to include the two jars and the `java/` directory (see Configuration)

Example SQL usage after installation:
```sql
INSTALL PLUGIN thai_ftparser SONAME 'libthai_ftparser.so';
CREATE FULLTEXT INDEX ft_thai_idx ON my_table(thai_content) WITH PARSER thai_ftparser;
SELECT * FROM my_table WHERE MATCH(thai_content) AGAINST('สวัสดี' IN NATURAL LANGUAGE MODE);
```

## 🧩 Plugin Installation

Refer to the official guide: OceanBase Plugin Development Kit User Guide: <https://oceanbase.github.io/oceanbase-plugin-dev-kit/user-guide/>

1) Prepare the shared library

- Build output: `build/libthai_ftparser.so`
- Registered name in OceanBase: `thai_ftparser` (used in `WITH PARSER thai_ftparser` and visible in `GV$OB_PLUGINS.NAME`)

2) Distribute the library

Copy the `.so` to each Observer node's `plugin_dir/`:
```bash
scp build/libthai_ftparser.so user@observer_node:/path/to/observer/plugin_dir/
```

3) Configure plugin loading

- CLI args: `-L` or `--plugins_load`
- Config item: `plugins_load`

Examples:
```bash
observer -L "libthai_ftparser.so:on"
# or
observer --plugins_load "libthai_ftparser.so:on"
```

SQL config example (requires sys tenant; needs restart):
```sql
alter system set plugins_load='libthai_ftparser.so:on';
```

4) Ensure all nodes are consistent

Every Observer node must have the same `plugin_dir/` content and loading config.

5) Restart Observer (if using config item approach)
```bash
killall observer
cd /path/to/observer
./bin/observer
```

6) Verify installation

```sql
select * from oceanbase.GV$OB_PLUGINS;
```

You should see `thai_ftparser`. If not shown, check `alert.log`/`observer.log` around `OB_SERVER_LOAD_DYNAMIC_PLUGIN_FAIL`, or search by the TraceId for detailed causes.

## 🏗️ Architecture

### Four-Layer Design

1. **Plugin Interface Layer** (`thai_ftparser_main.cpp`)
   - Implements OceanBase FTParser callbacks
   - Handles plugin lifecycle and registration
   - Provides C-compatible interface

2. **Core Parser Layer** (`thai_parser_core.cpp`)
   - Manages parsing sessions and token caching
   - Handles UTF-8 Thai character processing
   - Integrates with JNI bridge

3. **JNI Bridge Layer** (`thai_jni_bridge.cpp`)
   - Single JVM reused (centralized management)
   - Per-thread `JNIEnv*` attach/detach automatically
   - `PushLocalFrame/PopLocalFrame` for local reference management
   - Java exception capture and error propagation

4. **Java Segmentation Layer** (`java/`)
   - Implements actual Thai segmentation logic
   - Can integrate any Java-based segmentation library
   - Extensible and replaceable

### Data Flow

```
Thai Text Input
      ↓
OceanBase FTParser Interface
      ↓
Core Parser (C++)
      ↓
JNI Bridge (C++ → Java)
      ↓
Java Segmenter
      ↓
Third-party Thai Library
      ↓
Tokens (Java → C++)
      ↓
OceanBase Index
```

## 🧪 Testing

This repo does not include standalone test executables. Validate inside OceanBase (see installation and usage above).

## ⚡ Performance

- Single JVM reused; thread-safe parallel segmentation (per-thread `JNIEnv*`)
- JVM heap size configurable (see Configuration)

## 🔧 Configuration

### JVM / Classpath
`JNIBridgeConfig` resolves classpath as:
- Prefer environment variable `THAI_PARSER_CLASSPATH`
- Otherwise use built-in fallback paths (explicit jar filenames; JNI does not expand `*`)

Example (Observer):
```bash
export THAI_PARSER_CLASSPATH="/root/observer/java/lib/lucene-core-8.11.2.jar:/root/observer/java/lib/lucene-analyzers-common-8.11.2.jar:/root/observer/java"
```

Key parameters (see `thai_jni_bridge.h`):
```cpp
struct JNIBridgeConfig {
    std::string java_class_path;              // from THAI_PARSER_CLASSPATH or fallback
    std::string segmenter_class_name = "RealThaiSegmenter";
    std::string segment_method_name = "segment";
    size_t jvm_max_heap_mb = 512;
    size_t jvm_init_heap_mb = 128;
};
```

To replace the segmenter, either implement a Java class with the same name or change the C++ `segmenter_class_name`, and ensure all .class/.jar are on the classpath.

## 🐛 Troubleshooting

1) Plugin not loaded
```bash
objdump -T build/libthai_ftparser.so | grep thai_ftparser
```

2) JVM create/attach fails
```bash
echo $JAVA_HOME
echo $LD_LIBRARY_PATH
echo $THAI_PARSER_CLASSPATH
```

3) Class not found (ClassNotFoundException)
```bash
ls -l ${OB_WORKDIR}/java/RealThaiSegmenter.class
ls -l ${OB_WORKDIR}/java/lib/*.jar
# Or validate classpath with local Java
java -cp "$THAI_PARSER_CLASSPATH" -Xverify:none -version
```

4) Common error codes and symptoms

- `-11078` (plugin error/class not found): typically due to misconfigured classpath or use of wildcards. Fix:
  - Do not use `*`; explicitly list every jar
  - Ensure `RealThaiSegmenter.class` and both Lucene jars are present on `THAI_PARSER_CLASSPATH`
  - Validate under Observer working dir: `ls ${OB_WORKDIR}/java/...`, `java -cp "$THAI_PARSER_CLASSPATH" -version`

### Classpath notes

- JNI `-Djava.class.path` does not expand `*`. Every jar must be listed explicitly
- Include three entries:
  - lucene-core-8.11.2.jar
  - lucene-analyzers-common-8.11.2.jar
  - directory containing `RealThaiSegmenter.class` (e.g., `${OB_WORKDIR}/java`)

### Concurrency and thread safety

- Single JVM reused
- Per-thread `JNIEnv*` (`thread_local`), auto attach/detach
- `PushLocalFrame/PopLocalFrame` to manage local refs and avoid leaks

### Observer deployment checklist

```bash
# 1) Plugin library
ls -l ${OB_WORKDIR}/plugin_dir/libthai_ftparser.so

# 2) Java class and jars
ls -l ${OB_WORKDIR}/java/RealThaiSegmenter.class
ls -l ${OB_WORKDIR}/java/lib/lucene-*.jar

# 3) Classpath env (or rely on fallback paths)
echo $THAI_PARSER_CLASSPATH

# 4) Quick Java loadability check (local)
java -cp "$THAI_PARSER_CLASSPATH" -Xverify:none -version
```

### Core dump debugging tips

If Observer crashes with a core:
```bash
gdb /path/to/observer /path/to/core.xxx
(gdb) bt
```
Typical issues around `JNIEnv_::NewStringUTF` come from cross-thread `JNIEnv*` misuse or unbounded local refs. The current design uses per-thread `JNIEnv*` and `PushLocalFrame` to avoid these.

### Debug Mode
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
gdb -p <observer-pid>
```

## 📈 Development Workflow

1. Make changes
2. Rebuild: `cd build && make`
3. Validate in OB (install plugin, create index, run queries)
4. Debug with gdb on Observer if needed
5. Deploy `libthai_ftparser.so`

## 📚 Documentation

- OceanBase Plugin Development Kit User Guide: <https://oceanbase.github.io/oceanbase-plugin-dev-kit/user-guide/>
- JNI Programming Guide: <https://docs.oracle.com/javase/8/docs/technotes/guides/jni/>

## 🤝 Contributing

1. Fork the repository
2. Create feature branch
3. Make changes with tests
4. Ensure all tests pass
5. Submit pull request

## 📄 License

Licensed under the Apache License, Version 2.0. See source files for details.

## 🙏 Acknowledgments

- OceanBase team for plugin system design
- Thai language processing community
- Java segmentation library authors

---

**Status**: ✅ Ready  
**Version**: 1.0.0  
**Last Updated**: August 2025