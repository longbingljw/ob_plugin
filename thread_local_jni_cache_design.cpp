/**
 * 方案一：线程级别的JNI环境缓存
 * 在保持引用计数安全性的基础上，添加线程本地缓存
 */

namespace oceanbase {
namespace jni {

/**
 * 线程本地JNI环境缓存
 * 每个线程维护一个JNI环境的缓存，避免频繁attach/detach
 */
class ThreadLocalJNICache {
public:
    /**
     * 获取当前线程的缓存JNI环境
     * @param plugin_name 插件名称
     * @return 缓存的JNI环境，如果没有则返回nullptr
     */
    static JNIEnv* get_cached_env(const std::string& plugin_name);
    
    /**
     * 设置当前线程的缓存JNI环境
     * @param plugin_name 插件名称
     * @param env JNI环境指针
     */
    static void set_cached_env(const std::string& plugin_name, JNIEnv* env);
    
    /**
     * 清除当前线程的缓存
     * @param plugin_name 插件名称
     */
    static void clear_cached_env(const std::string& plugin_name);
    
    /**
     * 检查缓存的环境是否仍然有效
     * @param env 要检查的JNI环境
     * @return true如果有效，false如果无效
     */
    static bool is_env_valid(JNIEnv* env);

private:
    // 线程本地存储：插件名 -> JNI环境
    static thread_local std::unordered_map<std::string, JNIEnv*> cached_envs_;
    
    // 线程本地存储：缓存的JVM指针
    static thread_local JavaVM* cached_jvm_;
};

/**
 * 增强版的ScopedJNIEnvironment
 * 支持线程级别缓存，减少attach/detach开销
 */
class CachedScopedJNIEnvironment {
private:
    JNIEnv* env_;
    std::string plugin_name_;
    bool is_valid_;
    bool is_cached_; // 标记是否使用了缓存
    
public:
    CachedScopedJNIEnvironment(const std::string& plugin_name, 
                              const std::string& classpath = "",
                              size_t max_heap_mb = 512,
                              size_t init_heap_mb = 128);
    
    ~CachedScopedJNIEnvironment();
    
    JNIEnv* get() const { return env_; }
    operator bool() const { return is_valid_; }
    bool is_valid() const { return is_valid_; }
    
    // 禁用拷贝和移动
    CachedScopedJNIEnvironment(const CachedScopedJNIEnvironment&) = delete;
    CachedScopedJNIEnvironment& operator=(const CachedScopedJNIEnvironment&) = delete;
};

// 实现示例
thread_local std::unordered_map<std::string, JNIEnv*> ThreadLocalJNICache::cached_envs_;
thread_local JavaVM* ThreadLocalJNICache::cached_jvm_ = nullptr;

CachedScopedJNIEnvironment::CachedScopedJNIEnvironment(
    const std::string& plugin_name, 
    const std::string& classpath,
    size_t max_heap_mb,
    size_t init_heap_mb) 
    : env_(nullptr), plugin_name_(plugin_name), is_valid_(false), is_cached_(false) {
    
    // 1. 尝试从缓存获取
    env_ = ThreadLocalJNICache::get_cached_env(plugin_name);
    if (env_ && ThreadLocalJNICache::is_env_valid(env_)) {
        is_valid_ = true;
        is_cached_ = true;
        OBP_LOG_INFO("[%s] Using cached JNI environment", plugin_name.c_str());
        return;
    }
    
    // 2. 缓存失效或不存在，使用原始逻辑
    JavaVM* jvm = nullptr;
    if (!classpath.empty()) {
        jvm = GlobalJVMManager::get_or_create_jvm(classpath, max_heap_mb, init_heap_mb);
    } else {
        jvm = GlobalJVMManager::get_jvm();
    }
    
    if (jvm) {
        env_ = GlobalThreadManager::acquire_jni_env_for_plugin(jvm, plugin_name);
        is_valid_ = (env_ != nullptr);
        
        if (is_valid_) {
            // 3. 缓存新获取的环境
            ThreadLocalJNICache::set_cached_env(plugin_name, env_);
            is_cached_ = false; // 这次是新获取的，不是缓存的
            OBP_LOG_INFO("[%s] Acquired new JNI environment and cached it", plugin_name.c_str());
        }
    }
}

CachedScopedJNIEnvironment::~CachedScopedJNIEnvironment() {
    if (env_ && !is_cached_) {
        // 只有非缓存的环境才需要释放引用计数
        // 缓存的环境保持引用计数，下次可以直接使用
        JavaVM* jvm = GlobalJVMManager::get_jvm();
        if (jvm) {
            GlobalThreadManager::release_jni_env_for_plugin(jvm, plugin_name_);
        }
    }
    // 缓存的环境不释放，保持线程附着状态
}

} // namespace jni
} // namespace oceanbase
