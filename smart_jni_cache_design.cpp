/**
 * 方案三：智能引用计数缓存
 * 在现有引用计数基础上添加智能缓存机制
 */

namespace oceanbase {
namespace jni {

/**
 * 增强版的GlobalThreadManager
 * 支持智能缓存，减少不必要的attach/detach
 */
class SmartGlobalThreadManager {
public:
    /**
     * 获取JNI环境（智能缓存版本）
     * @param jvm JVM实例
     * @param plugin_name 插件名称
     * @param cache_hint 缓存提示：true表示希望缓存，false表示立即释放
     * @return JNI环境指针
     */
    static JNIEnv* acquire_jni_env_smart(JavaVM* jvm, const std::string& plugin_name, bool cache_hint = true);
    
    /**
     * 释放JNI环境（智能缓存版本）
     * @param jvm JVM实例
     * @param plugin_name 插件名称
     * @param force_detach 强制分离：true表示立即分离，false表示根据策略决定
     */
    static void release_jni_env_smart(JavaVM* jvm, const std::string& plugin_name, bool force_detach = false);
    
    /**
     * 设置线程缓存策略
     * @param thread_id 线程ID
     * @param keep_alive_ms 保持存活时间（毫秒），0表示立即释放
     */
    static void set_thread_cache_policy(std::thread::id thread_id, int keep_alive_ms);

private:
    struct ThreadCacheInfo {
        int ref_count;
        std::chrono::steady_clock::time_point last_access;
        int keep_alive_ms;
        std::unordered_set<std::string> active_plugins;
    };
    
    static std::mutex smart_thread_mutex_;
    static std::unordered_map<std::thread::id, ThreadCacheInfo> thread_cache_info_;
    static std::unordered_set<std::thread::id> attached_threads_;
    
    /**
     * 清理过期的缓存线程
     */
    static void cleanup_expired_threads();
    
    /**
     * 检查线程是否应该保持附着
     */
    static bool should_keep_thread_attached(std::thread::id thread_id);
};

/**
 * 智能ScopedJNIEnvironment
 * 支持缓存策略的RAII JNI环境管理
 */
class SmartScopedJNIEnvironment {
private:
    JNIEnv* env_;
    std::string plugin_name_;
    bool is_valid_;
    bool cache_enabled_;
    
public:
    /**
     * 构造函数
     * @param plugin_name 插件名称
     * @param classpath Java类路径
     * @param cache_enabled 是否启用缓存
     * @param max_heap_mb 最大堆内存
     * @param init_heap_mb 初始堆内存
     */
    SmartScopedJNIEnvironment(const std::string& plugin_name, 
                             const std::string& classpath = "",
                             bool cache_enabled = true,
                             size_t max_heap_mb = 512,
                             size_t init_heap_mb = 128);
    
    ~SmartScopedJNIEnvironment();
    
    JNIEnv* get() const { return env_; }
    operator bool() const { return is_valid_; }
    bool is_valid() const { return is_valid_; }
    
    /**
     * 设置当前操作的缓存提示
     * @param should_cache true表示建议缓存，false表示建议立即释放
     */
    void set_cache_hint(bool should_cache) { cache_enabled_ = should_cache; }
};

// 实现示例
SmartScopedJNIEnvironment::SmartScopedJNIEnvironment(
    const std::string& plugin_name, 
    const std::string& classpath,
    bool cache_enabled,
    size_t max_heap_mb,
    size_t init_heap_mb) 
    : env_(nullptr), plugin_name_(plugin_name), is_valid_(false), cache_enabled_(cache_enabled) {
    
    JavaVM* jvm = nullptr;
    if (!classpath.empty()) {
        jvm = GlobalJVMManager::get_or_create_jvm(classpath, max_heap_mb, init_heap_mb);
    } else {
        jvm = GlobalJVMManager::get_jvm();
    }
    
    if (jvm) {
        env_ = SmartGlobalThreadManager::acquire_jni_env_smart(jvm, plugin_name, cache_enabled_);
        is_valid_ = (env_ != nullptr);
    }
}

SmartScopedJNIEnvironment::~SmartScopedJNIEnvironment() {
    if (env_) {
        JavaVM* jvm = GlobalJVMManager::get_jvm();
        if (jvm) {
            SmartGlobalThreadManager::release_jni_env_smart(jvm, plugin_name_, !cache_enabled_);
        }
    }
}

} // namespace jni
} // namespace oceanbase

/**
 * 使用示例：插件中的智能缓存
 */
namespace oceanbase {
namespace japanese_ftparser {

class OptimizedJapaneseJNIBridge {
public:
    int segment(const std::string& text, std::vector<std::string>& tokens) {
        // 启用智能缓存，减少attach/detach
        oceanbase::jni::SmartScopedJNIEnvironment jni_env(plugin_name_, "", true);
        
        if (!jni_env) {
            set_error(OBP_PLUGIN_ERROR, "Failed to acquire JNI environment");
            return OBP_PLUGIN_ERROR;
        }
        
        return do_segment(jni_env.get(), text, tokens);
    }
    
    int batch_segment(const std::vector<std::string>& texts, 
                     std::vector<std::vector<std::string>>& results) {
        // 批量操作，强烈建议缓存
        oceanbase::jni::SmartScopedJNIEnvironment jni_env(plugin_name_, "", true);
        
        if (!jni_env) {
            return OBP_PLUGIN_ERROR;
        }
        
        results.resize(texts.size());
        for (size_t i = 0; i < texts.size(); ++i) {
            // 在同一个JNI环境中处理多个文本，无额外attach/detach开销
            int ret = do_segment(jni_env.get(), texts[i], results[i]);
            if (ret != OBP_SUCCESS) {
                return ret;
            }
        }
        
        return OBP_SUCCESS;
    }
};

} // namespace japanese_ftparser
} // namespace oceanbase
