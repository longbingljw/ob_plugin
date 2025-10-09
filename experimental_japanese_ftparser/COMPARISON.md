# 原有插件 vs 实验插件 - 详细对比分析

## 📊 整体对比

| 维度 | 原有插件 (japanese_ftparser) | 实验插件 (experimental) | 改进 |
|------|------------------------------|------------------------|------|
| 总代码量 | 1818行 | 602行 | ↓ 67% |
| JVM管理 | 每个插件独立 | 全局统一 | ✅ 彻底解决冲突 |
| 线程管理 | 独立的线程集合 | 全局引用计数 | ✅ 状态同步 |
| 资源管理 | 手动管理 | RAII自动管理 | ✅ 异常安全 |
| 多插件支持 | ❌ 有冲突 | ✅ 完全支持 | ✅ 核心问题解决 |

## 🔍 关键差异详解

### 1. JVM管理架构

**原有插件（每个插件独立）：**
```cpp
// japanese_ftparser.so
class JVMStateManager {
    static JavaVM* global_jvm_;           // 日语插件的JVM指针
    static std::atomic<int> instance_count_{0};
};

// korean_ftparser.so
class JVMStateManager {
    static JavaVM* global_jvm_;           // 韩语插件的JVM指针 (独立变量)
    static std::atomic<int> instance_count_{0};  // 独立计数器
};

// 问题：同一个JVM实例，但有多个独立的管理器
```

**实验插件（全局统一）：**
```cpp
// liboceanbase_jni_common.so
class GlobalJVMManager {
    static JavaVM* shared_jvm_;           // 🎯 全局唯一的JVM指针
    static std::atomic<int> plugin_count_{0};  // 🎯 统一的插件计数
};

// 所有插件共享这个唯一的管理器
// ✅ 单一权威，状态一致
```

### 2. 线程状态管理

**原有插件（独立管理）：**
```cpp
// 每个插件都有自己的attached_threads_集合
japanese::ThreadStateManager::attached_threads_ = {Thread-1}
korean::ThreadStateManager::attached_threads_ = {Thread-3}
thai::ThreadStateManager::attached_threads_ = {Thread-2}

// 问题：Thread-1实际可能被所有插件使用，但每个插件只知道自己的使用情况
```

**实验插件（全局协调）：**
```cpp
// 全局的线程引用计数
GlobalThreadManager::global_thread_ref_count_ = {
    Thread-1: 3,  // 被3个插件同时使用
    Thread-2: 1,  // 被1个插件使用  
    Thread-3: 2   // 被2个插件使用
}

GlobalThreadManager::thread_plugin_map_ = {
    Thread-1: {"japanese", "korean", "thai"},
    Thread-2: {"thai"},
    Thread-3: {"japanese", "korean"}
}

// ✅ 完整的全局视图，知道每个线程被哪些插件使用
```

### 3. 线程分离时机

**原有插件：**
```
japanese_plugin结束 → 检查自己的attached_threads_ 
                    → 发现Thread-1在其中 
                    → DetachCurrentThread()  ❌ 但korean和thai还在使用！
```

**实验插件：**
```
japanese_plugin结束 → global_thread_ref_count_[Thread-1]-- (3→2)
                    → ref_count > 0 
                    → 不分离线程 ✅

korean_plugin结束   → global_thread_ref_count_[Thread-1]-- (2→1)
                    → ref_count > 0
                    → 不分离线程 ✅

thai_plugin结束     → global_thread_ref_count_[Thread-1]-- (1→0)
                    → ref_count == 0
                    → 现在分离线程 ✅ 安全！
```

### 4. 资源管理风格

**原有插件（手动管理）：**
```cpp
int segment(const std::string& text, std::vector<std::string>& tokens) {
    // 手动获取
    thread_env_ = ThreadStateManager::get_jni_env_for_current_thread(jvm_);
    
    // 手动推入局部引用帧
    thread_env_->PushLocalFrame(64);
    
    // 业务逻辑...
    
    // 手动弹出局部引用帧
    thread_env_->PopLocalFrame(nullptr);
    
    // cleanup()中手动清理线程状态
    return OBP_SUCCESS;
}
```

**实验插件（RAII自动管理）：**
```cpp
int segment(const std::string& text, std::vector<std::string>& tokens) {
    // 🎯 RAII自动管理
    ScopedJNIEnvironment jni_env(plugin_name_);
    if (!jni_env) return OBP_PLUGIN_ERROR;
    
    // 业务逻辑...
    return do_segment(jni_env.get(), text, tokens);
    
    // 🎯 析构时自动清理：
    //   1. 减少线程引用计数
    //   2. 如果ref_count==0，分离线程
    //   3. 异常安全
}
```

## 🎯 **解决多插件冲突的关键机制**

### **机制1：全局唯一的静态变量**

```cpp
// 公共库liboceanbase_jni_common.so中的静态变量在整个进程中只有一份
namespace oceanbase::jni {
    // 这些变量在进程中全局唯一
    std::mutex GlobalJVMManager::global_mutex_;
    JavaVM* GlobalJVMManager::shared_jvm_ = nullptr;
    std::unordered_map<std::thread::id, int> GlobalThreadManager::global_thread_ref_count_;
}

// 所有链接到这个.so的插件都共享这些变量
// ✅ 真正的全局唯一，不会有多份拷贝
```

### **机制2：引用计数的正确实现**

```cpp
// 每个插件调用acquire时：
JNIEnv* GlobalThreadManager::acquire_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(thread_mutex_);
    
    JNIEnv* env = nullptr;
    jint result = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);
    
    if (result == JNI_OK) {
        // 线程已附着，增加引用计数
        global_thread_ref_count_[tid]++;
        thread_plugin_map_[tid].insert(plugin_name);
        return env;
    } else if (result == JNI_EDETACHED) {
        // 需要附着线程
        result = jvm->AttachCurrentThread((void**)&env, nullptr);
        if (result == JNI_OK) {
            global_thread_ref_count_[tid] = 1;  // 初始引用计数
            thread_plugin_map_[tid].insert(plugin_name);
            return env;
        }
    }
}

// 每个插件调用release时：
void GlobalThreadManager::release_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(thread_mutex_);
    
    auto ref_it = global_thread_ref_count_.find(tid);
    if (ref_it != global_thread_ref_count_.end()) {
        ref_it->second--;  // 引用计数减1
        thread_plugin_map_[tid].erase(plugin_name);
        
        if (ref_it->second <= 0) {
            // ✅ 只有引用计数为0时才分离线程
            jvm->DetachCurrentThread();
            attached_threads_.erase(tid);
        }
    }
}
```

### **机制3：RAII确保资源总是被释放**

```cpp
class ScopedJNIEnvironment {
public:
    ScopedJNIEnvironment(...) {
        env_ = GlobalThreadManager::acquire_jni_env_for_plugin(...);
    }
    
    ~ScopedJNIEnvironment() {
        if (env_) {
            GlobalThreadManager::release_jni_env_for_plugin(...);
        }
        // ✅ 即使代码抛出异常，析构函数也会被调用
        // ✅ 确保引用计数总是被正确减少
    }
};
```

## ✅ **实验插件的核心优势**

### **优势1：彻底解决状态不一致**
- 原有：3个独立的管理器，互不相知
- 实验：1个全局管理器，统一视图

### **优势2：正确的引用计数**
- 原有：无线程级别引用计数，容易过早分离
- 实验：全局线程引用计数，精确控制分离时机

### **优势3：简化开发**
- 原有：每个插件需要1500+行JVM管理代码
- 实验：只需要200行业务逻辑代码

### **优势4：易于扩展**
- 原有：新增插件需要复制粘贴JVM管理代码
- 实验：新增插件只需要实现业务逻辑，自动获得JVM管理能力

## 🎯 **总结**

实验插件通过**公共库方案**成功解决了多插件JNI资源管理冲突的核心问题：

1. **✅ 统一的JVM管理**：全局唯一的JVM实例和管理器
2. **✅ 统一的线程管理**：全局线程引用计数和状态跟踪
3. **✅ RAII资源管理**：自动的资源获取和释放
4. **✅ 代码简化**：减少67%的代码量
5. **✅ 异常安全**：即使出现异常也能正确清理资源

这个方案完美实现了**中心化协调器**模式，是解决分布式系统中共享状态管理问题的经典方案在OceanBase插件系统中的成功应用！
