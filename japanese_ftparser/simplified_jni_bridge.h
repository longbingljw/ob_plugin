/**
 * Copyright (c) 2023 OceanBase
 * Experimental Japanese Fulltext Parser Plugin - Simplified Version
 */

#pragma once

#include "oceanbase/ob_plugin_ftparser.h"
#include "jni_manager.h"  // 简化后的包含路径
#include <string>
#include <vector>
#include <mutex>

namespace oceanbase {
namespace japanese_ftparser {

/**
 * Simplified JNI Bridge Configuration
 */
struct SimplifiedJNIBridgeConfig {
    std::string java_class_path;
    std::string segmenter_class_name;
    std::string segment_method_name;
    size_t jvm_max_heap_mb;
    size_t jvm_init_heap_mb;
    
    SimplifiedJNIBridgeConfig();
};

/**
 * Simplified JNI Bridge for Japanese Segmentation
 * @brief Simplified version using the common JNI library
 * @details This class uses oceanbase::jni::ScopedJNIEnvironment for
 * automatic JVM and thread management, eliminating the need for
 * complex manual resource management.
 */
class SimplifiedJNIBridge {
private:
    SimplifiedJNIBridgeConfig config_;
    std::string plugin_name_;
    bool is_initialized_;
    std::mutex bridge_mutex_;
    
    // Java class and method references (cached for performance)
    jclass segmenter_class_;
    jmethodID constructor_method_;
    jmethodID segment_method_;
    
    // Error handling
    struct ErrorInfo {
        int error_code;
        std::string error_message;
        std::string java_exception;
    };
    ErrorInfo last_error_;
    
public:
    /**
     * Constructor
     */
    SimplifiedJNIBridge();
    
    /**
     * Destructor
     */
    ~SimplifiedJNIBridge();
    
    /**
     * Initialize the simplified JNI bridge
     * @return OBP_SUCCESS on success, error code on failure
     */
    int initialize();
    
    /**
     * Segment text into tokens
     * @param text Input text to segment
     * @param tokens Output vector to store tokens
     * @return OBP_SUCCESS on success, error code on failure
     */
    int segment(const std::string& text, std::vector<std::string>& tokens);
    
    /**
     * Get last error information
     */
    const ErrorInfo& get_last_error() const { return last_error_; }
    
    /**
     * Check if bridge is initialized
     */
    bool is_initialized() const { return is_initialized_; }

private:
    /**
     * Load Java classes and cache method IDs
     */
    int load_java_classes(JNIEnv* env);
    
    /**
     * Perform actual segmentation with given JNI environment
     */
    int do_segment(JNIEnv* env, const std::string& text, std::vector<std::string>& tokens);
    
    /**
     * Set error information
     */
    void set_error(int code, const std::string& message);
    
    /**
     * Clear error information
     */
    void clear_error();
    
    // Disable copy and move
    SimplifiedJNIBridge(const SimplifiedJNIBridge&) = delete;
    SimplifiedJNIBridge& operator=(const SimplifiedJNIBridge&) = delete;
};

/**
 * Simplified JNI Bridge Manager (Singleton)
 * @brief Manages a global simplified JNI bridge instance
 */
class SimplifiedJNIBridgeManager {
public:
    /**
     * Get the singleton instance
     */
    static SimplifiedJNIBridgeManager& get_instance();
    
    /**
     * Get the JNI bridge
     */
    std::shared_ptr<SimplifiedJNIBridge> get_bridge();
    
    /**
     * Initialize the bridge (thread-safe)
     */
    int initialize();

private:
    std::shared_ptr<SimplifiedJNIBridge> bridge_;
    std::mutex mutex_;
    
    SimplifiedJNIBridgeManager() = default;
    ~SimplifiedJNIBridgeManager() = default;
    
    // Disable copy
    SimplifiedJNIBridgeManager(const SimplifiedJNIBridgeManager&) = delete;
    SimplifiedJNIBridgeManager& operator=(const SimplifiedJNIBridgeManager&) = delete;
};

} // namespace japanese_ftparser
} // namespace oceanbase

// Plugin interface functions (declarations only)
extern "C" {
    int japanese_ftparser_init(ObPluginParamPtr param);
    int japanese_ftparser_deinit(ObPluginParamPtr param);
    int japanese_ftparser_scan_begin(ObPluginFTParserParamPtr param);
    int japanese_ftparser_scan_end(ObPluginFTParserParamPtr param);
    int japanese_ftparser_next_token(ObPluginFTParserParamPtr param, char **word, int64_t *word_len, int64_t *char_cnt, int64_t *word_freq);
    int japanese_ftparser_get_add_word_flag(uint64_t *flag);
}