/**
 * 立即可行的快速优化方案
 * 在现有GlobalThreadManager基础上添加简单的线程缓存
 */

namespace oceanbase {
namespace jni {

// 在GlobalThreadManager中添加缓存逻辑
class GlobalThreadManager {
private:
    // 现有成员...
    static std::mutex thread_mutex_;
    static std::unordered_map<std::thread::id, int> global_thread_ref_count_;
    static std::unordered_set<std::thread::id> attached_threads_;
    
    // 新增：线程缓存信息
    struct ThreadCacheEntry {
        std::chrono::steady_clock::time_point last_access;
        int idle_count; // 连续空闲次数
    };
    static std::unordered_map<std::thread::id, ThreadCacheEntry> thread_cache_;
    static const int MAX_IDLE_COUNT = 5; // 最大空闲次数
    static const int CACHE_TIMEOUT_MS = 1000; // 缓存超时时间

public:
    // 现有方法...
    static JNIEnv* acquire_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name);
    static void release_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name);
    
    // 新增：带缓存的释放方法
    static void release_jni_env_for_plugin_cached(JavaVM* jvm, const std::string& plugin_name) {
        if (!jvm) return;
        
        std::lock_guard<std::mutex> lock(thread_mutex_);
        std::thread::id current_thread_id = std::this_thread::get_id();
        
        auto ref_it = global_thread_ref_count_.find(current_thread_id);
        if (ref_it != global_thread_ref_count_.end()) {
            ref_it->second--;
            
            if (ref_it->second <= 0) {
                // 引用计数为0，但不立即detach，而是缓存一段时间
                auto now = std::chrono::steady_clock::now();
                thread_cache_[current_thread_id] = {now, 0};
                
                OBP_LOG_INFO("[%s] Thread %p cached for potential reuse", 
                           plugin_name.c_str(), &current_thread_id);
            }
        }
    }
    
    // 新增：清理过期缓存
    static void cleanup_expired_cache() {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        auto it = thread_cache_.begin();
        while (it != thread_cache_.end()) {
            auto thread_id = it->first;
            auto& cache_entry = it->second;
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - cache_entry.last_access).count();
            
            if (elapsed > CACHE_TIMEOUT_MS || cache_entry.idle_count > MAX_IDLE_COUNT) {
                // 超时或空闲次数过多，真正detach
                if (attached_threads_.count(thread_id) > 0) {
                    // 注意：这里需要在正确的线程上下文中调用DetachCurrentThread
                    // 实际实现中可能需要更复杂的线程间通信
                    OBP_LOG_INFO("Thread %p detached due to cache expiry", &thread_id);
                    attached_threads_.erase(thread_id);
                }
                global_thread_ref_count_.erase(thread_id);
                it = thread_cache_.erase(it);
            } else {
                cache_entry.idle_count++;
                ++it;
            }
        }
    }
};

// 修改后的ScopedJNIEnvironment，支持缓存
class CachedScopedJNIEnvironment {
private:
    JNIEnv* env_;
    std::string plugin_name_;
    bool is_valid_;
    
public:
    CachedScopedJNIEnvironment(const std::string& plugin_name, 
                              const std::string& classpath = "",
                              size_t max_heap_mb = 512,
                              size_t init_heap_mb = 128) 
        : env_(nullptr), plugin_name_(plugin_name), is_valid_(false) {
        
        JavaVM* jvm = nullptr;
        if (!classpath.empty()) {
            jvm = GlobalJVMManager::get_or_create_jvm(classpath, max_heap_mb, init_heap_mb);
        } else {
            jvm = GlobalJVMManager::get_jvm();
        }
        
        if (jvm) {
            env_ = GlobalThreadManager::acquire_jni_env_for_plugin(jvm, plugin_name);
            is_valid_ = (env_ != nullptr);
        }
    }
    
    ~CachedScopedJNIEnvironment() {
        if (env_) {
            JavaVM* jvm = GlobalJVMManager::get_jvm();
            if (jvm) {
                // 使用缓存版本的释放方法
                GlobalThreadManager::release_jni_env_for_plugin_cached(jvm, plugin_name_);
            }
        }
    }
    
    JNIEnv* get() const { return env_; }
    operator bool() const { return is_valid_; }
    bool is_valid() const { return is_valid_; }
};

} // namespace jni
} // namespace oceanbase

// 使用示例：插件中只需要改一行代码
namespace oceanbase {
namespace japanese_ftparser {

int JapaneseJNIBridge::segment(const std::string& text, std::vector<std::string>& tokens) {
    clear_error();
    
    // 从 ScopedJNIEnvironment 改为 CachedScopedJNIEnvironment
    oceanbase::jni::CachedScopedJNIEnvironment jni_env(plugin_name_);
    
    if (!jni_env) {
        set_error(OBP_PLUGIN_ERROR, "Failed to acquire JNI environment for segmentation");
        return OBP_PLUGIN_ERROR;
    }
    
    return do_segment(jni_env.get(), text, tokens);
}

} // namespace japanese_ftparser
} // namespace oceanbase
