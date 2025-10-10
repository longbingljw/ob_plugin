/**
 * 方案二：插件级别的JNI环境持有
 * 插件在初始化时获取JNI环境，在生命周期内持有
 */

namespace oceanbase {
namespace japanese_ftparser {

/**
 * 持久化JNI环境管理器
 * 为插件提供长期持有的JNI环境
 */
class PersistentJNIEnvironment {
private:
    std::string plugin_name_;
    JNIEnv* env_;
    JavaVM* jvm_;
    bool is_valid_;
    std::mutex env_mutex_; // 保护JNI环境访问
    
public:
    PersistentJNIEnvironment(const std::string& plugin_name, 
                           const std::string& classpath,
                           size_t max_heap_mb = 512,
                           size_t init_heap_mb = 128);
    
    ~PersistentJNIEnvironment();
    
    /**
     * 获取JNI环境（线程安全）
     * @return JNI环境指针，如果无效则返回nullptr
     */
    JNIEnv* get_env();
    
    /**
     * 检查环境是否有效
     */
    bool is_valid() const { return is_valid_; }
    
    /**
     * 重新初始化环境（如果需要）
     */
    int reinitialize();
    
private:
    /**
     * 验证JNI环境是否仍然有效
     */
    bool validate_env();
};

/**
 * 修改后的JapaneseJNIBridge
 * 使用持久化JNI环境
 */
class JapaneseJNIBridge {
private:
    JapaneseJNIBridgeConfig config_;
    std::string plugin_name_;
    JNIErrorInfo last_error_;
    
    // 持久化JNI环境
    std::unique_ptr<PersistentJNIEnvironment> persistent_env_;
    
    // Java类和方法缓存
    jclass segmenter_class_;
    jmethodID constructor_method_;
    jmethodID segment_method_;
    
public:
    explicit JapaneseJNIBridge(const JapaneseJNIBridgeConfig& config = JapaneseJNIBridgeConfig());
    ~JapaneseJNIBridge();
    
    int initialize();
    int segment(const std::string& text, std::vector<std::string>& tokens);
    
private:
    int load_java_classes(JNIEnv* env);
    int do_segment(JNIEnv* env, const std::string& text, std::vector<std::string>& tokens);
};

// 实现示例
PersistentJNIEnvironment::PersistentJNIEnvironment(
    const std::string& plugin_name, 
    const std::string& classpath,
    size_t max_heap_mb,
    size_t init_heap_mb) 
    : plugin_name_(plugin_name), env_(nullptr), jvm_(nullptr), is_valid_(false) {
    
    // 获取或创建JVM
    jvm_ = oceanbase::jni::GlobalJVMManager::get_or_create_jvm(classpath, max_heap_mb, init_heap_mb);
    if (!jvm_) {
        return;
    }
    
    // 获取JNI环境并持有
    env_ = oceanbase::jni::GlobalThreadManager::acquire_jni_env_for_plugin(jvm_, plugin_name);
    is_valid_ = (env_ != nullptr);
    
    if (is_valid_) {
        OBP_LOG_INFO("[%s] Persistent JNI environment acquired", plugin_name.c_str());
    }
}

PersistentJNIEnvironment::~PersistentJNIEnvironment() {
    if (env_ && jvm_) {
        oceanbase::jni::GlobalThreadManager::release_jni_env_for_plugin(jvm_, plugin_name_);
        OBP_LOG_INFO("[%s] Persistent JNI environment released", plugin_name_.c_str());
    }
}

JNIEnv* PersistentJNIEnvironment::get_env() {
    std::lock_guard<std::mutex> lock(env_mutex_);
    
    // 验证环境是否仍然有效
    if (!validate_env()) {
        // 尝试重新初始化
        if (reinitialize() != OBP_SUCCESS) {
            return nullptr;
        }
    }
    
    return env_;
}

int JapaneseJNIBridge::segment(const std::string& text, std::vector<std::string>& tokens) {
    if (!persistent_env_ || !persistent_env_->is_valid()) {
        set_error(OBP_PLUGIN_ERROR, "Persistent JNI environment not available");
        return OBP_PLUGIN_ERROR;
    }
    
    // 获取JNI环境（无attach/detach开销！）
    JNIEnv* env = persistent_env_->get_env();
    if (!env) {
        set_error(OBP_PLUGIN_ERROR, "Failed to get JNI environment");
        return OBP_PLUGIN_ERROR;
    }
    
    return do_segment(env, text, tokens);
}

} // namespace japanese_ftparser
} // namespace oceanbase
