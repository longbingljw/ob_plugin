/**
 * Copyright (c) 2023 OceanBase
 * OceanBase JNI Common Library - Implementation
 */

#include "jni_manager.h"
#include <iostream>
#include <sstream>
#include <cstring>

// For logging - using simple cout for now, can be replaced with proper logging
#define OBP_LOG_INFO(fmt, ...) \
    do { \
        printf("[INFO][JNI_COMMON] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

#define OBP_LOG_WARN(fmt, ...) \
    do { \
        printf("[WARN][JNI_COMMON] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

#define OBP_LOG_ERROR(fmt, ...) \
    do { \
        printf("[ERROR][JNI_COMMON] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

namespace oceanbase {
namespace jni {

// GlobalJVMManager static members
std::mutex GlobalJVMManager::global_mutex_;
JavaVM* GlobalJVMManager::shared_jvm_ = nullptr;
std::atomic<int> GlobalJVMManager::plugin_count_{0};
bool GlobalJVMManager::jvm_created_by_us_ = false;
std::unordered_set<std::string> GlobalJVMManager::registered_plugins_;
std::string GlobalJVMManager::first_instance_classpath_;
size_t GlobalJVMManager::first_instance_max_heap_mb_ = 0;
size_t GlobalJVMManager::first_instance_init_heap_mb_ = 0;
bool GlobalJVMManager::config_recorded_ = false;

// GlobalThreadManager static members
std::mutex GlobalThreadManager::thread_mutex_;
std::unordered_map<std::thread::id, int> GlobalThreadManager::global_thread_ref_count_;
std::unordered_set<std::thread::id> GlobalThreadManager::attached_threads_;
std::unordered_map<std::thread::id, std::unordered_set<std::string>> GlobalThreadManager::thread_plugin_map_;

JavaVM* GlobalJVMManager::get_or_create_jvm(const std::string& classpath, 
                                           size_t max_heap_mb, 
                                           size_t init_heap_mb) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    
    // Validate configuration consistency (with warnings if mismatch)
    validate_config_consistency(classpath, max_heap_mb, init_heap_mb);
    
    // If JVM already exists, return it
    if (shared_jvm_) {
        OBP_LOG_INFO("Using existing global JVM instance");
        return shared_jvm_;
    }
    
    // Try to get existing JVM from the process
    jsize jvm_count = 0;
    jint result = JNI_GetCreatedJavaVMs(&shared_jvm_, 1, &jvm_count);
    
    if (result == JNI_OK && jvm_count > 0) {
        // Found existing JVM
        OBP_LOG_INFO("Found existing JVM in process, reusing it");
        jvm_created_by_us_ = false;
        return shared_jvm_;
    }
    
    // Create new JVM
    OBP_LOG_INFO("Creating new JVM with classpath: %s", classpath.c_str());
    
    JavaVMInitArgs vm_args;
    JavaVMOption options[5];
    
    // Create persistent copies of option strings to avoid dangling pointers
    static std::string classpath_option = "-Djava.class.path=" + classpath;
    static std::string max_heap_option = "-Xmx" + std::to_string(max_heap_mb) + "m";
    static std::string init_heap_option = "-Xms" + std::to_string(init_heap_mb) + "m";
    
    options[0].optionString = const_cast<char*>(classpath_option.c_str());
    options[1].optionString = const_cast<char*>(max_heap_option.c_str());
    options[2].optionString = const_cast<char*>(init_heap_option.c_str());
    options[3].optionString = const_cast<char*>("-XX:+UseG1GC");
    options[4].optionString = const_cast<char*>("-Dfile.encoding=UTF-8");
    
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 5;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;
    
    JNIEnv* env = nullptr;
    result = JNI_CreateJavaVM(&shared_jvm_, (void**)&env, &vm_args);
    
    if (result == JNI_OK) {
        jvm_created_by_us_ = true;
        OBP_LOG_INFO("JVM created successfully");
        return shared_jvm_;
    } else {
        OBP_LOG_ERROR("Failed to create JVM, error code: %d", result);
        shared_jvm_ = nullptr;
        return nullptr;
    }
}

void GlobalJVMManager::register_plugin(const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    
    if (registered_plugins_.insert(plugin_name).second) {
        int count = ++plugin_count_;
        OBP_LOG_INFO("Plugin '%s' registered, total count: %d", plugin_name.c_str(), count);
    } else {
        OBP_LOG_WARN("Plugin '%s' already registered", plugin_name.c_str());
    }
}

void GlobalJVMManager::unregister_plugin(const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    
    if (registered_plugins_.erase(plugin_name) > 0) {
        int count = --plugin_count_;
        OBP_LOG_INFO("Plugin '%s' unregistered, remaining count: %d", plugin_name.c_str(), count);
        
        if (count == 0) {
            OBP_LOG_INFO("Last plugin unregistered, keeping JVM alive for stability");
            // Don't destroy JVM for stability - let it stay alive
            // This prevents crashes in multi-threaded environments
        }
    } else {
        OBP_LOG_WARN("Plugin '%s' was not registered", plugin_name.c_str());
    }
}

int GlobalJVMManager::get_plugin_count() {
    return plugin_count_.load();
}

void GlobalJVMManager::force_shutdown_jvm() {
    std::lock_guard<std::mutex> lock(global_mutex_);
    if (shared_jvm_ && jvm_created_by_us_) {
        OBP_LOG_WARN("Force shutting down JVM");
        shared_jvm_->DestroyJavaVM();
        shared_jvm_ = nullptr;
        jvm_created_by_us_ = false;
    }
}

JavaVM* GlobalJVMManager::get_jvm() {
    std::lock_guard<std::mutex> lock(global_mutex_);
    return shared_jvm_;
}

bool GlobalJVMManager::validate_config_consistency(const std::string& classpath, 
                                                  size_t max_heap_mb, 
                                                  size_t init_heap_mb) {
    if (!config_recorded_) {
        // First instance - record the configuration
        first_instance_classpath_ = classpath;
        first_instance_max_heap_mb_ = max_heap_mb;
        first_instance_init_heap_mb_ = init_heap_mb;
        config_recorded_ = true;
        OBP_LOG_INFO("JVM configuration recorded: classpath=%s, max_heap=%zuMB, init_heap=%zuMB", 
                     classpath.c_str(), max_heap_mb, init_heap_mb);
        return true;
    }
    
    // Subsequent instances - validate consistency
    bool is_consistent = true;
    
    if (classpath != first_instance_classpath_) {
        OBP_LOG_WARN("JVM classpath mismatch detected:");
        OBP_LOG_WARN("  First instance: %s", first_instance_classpath_.c_str());
        OBP_LOG_WARN("  Current instance: %s", classpath.c_str());
        is_consistent = false;
    }
    
    if (max_heap_mb != first_instance_max_heap_mb_) {
        OBP_LOG_WARN("JVM max heap size mismatch: first=%zuMB, current=%zuMB", 
                     first_instance_max_heap_mb_, max_heap_mb);
        is_consistent = false;
    }
    
    if (init_heap_mb != first_instance_init_heap_mb_) {
        OBP_LOG_WARN("JVM init heap size mismatch: first=%zuMB, current=%zuMB", 
                     first_instance_init_heap_mb_, init_heap_mb);
        is_consistent = false;
    }
    
    return is_consistent;
}

JNIEnv* GlobalThreadManager::acquire_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name) {
    if (!jvm) {
        OBP_LOG_ERROR("JVM is null");
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(thread_mutex_);
    std::thread::id current_thread_id = std::this_thread::get_id();
    
    JNIEnv* env = nullptr;
    jint result = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);
    
    if (result == JNI_OK) {
        // Thread already attached, increase global reference count
        global_thread_ref_count_[current_thread_id]++;
        thread_plugin_map_[current_thread_id].insert(plugin_name);
        OBP_LOG_INFO("[%s] Thread %p already attached, global ref count: %d", 
                    plugin_name.c_str(), &current_thread_id, global_thread_ref_count_[current_thread_id]);
        return env;
    } else if (result == JNI_EDETACHED) {
        // Need to attach thread
        result = jvm->AttachCurrentThread((void**)&env, nullptr);
        if (result == JNI_OK) {
            attached_threads_.insert(current_thread_id);
            global_thread_ref_count_[current_thread_id] = 1;
            thread_plugin_map_[current_thread_id].insert(plugin_name);
            OBP_LOG_INFO("[%s] Thread %p attached to JVM, global ref count: 1", 
                        plugin_name.c_str(), &current_thread_id);
            return env;
        } else {
            OBP_LOG_ERROR("[%s] Failed to attach thread %p to JVM, error: %d", 
                         plugin_name.c_str(), &current_thread_id, result);
            return nullptr;
        }
    } else {
        OBP_LOG_ERROR("[%s] Unexpected JVM GetEnv result: %d", plugin_name.c_str(), result);
        return nullptr;
    }
}

void GlobalThreadManager::release_jni_env_for_plugin(JavaVM* jvm, const std::string& plugin_name) {
    if (!jvm) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(thread_mutex_);
    std::thread::id current_thread_id = std::this_thread::get_id();
    
    auto ref_it = global_thread_ref_count_.find(current_thread_id);
    if (ref_it != global_thread_ref_count_.end()) {
        ref_it->second--;
        thread_plugin_map_[current_thread_id].erase(plugin_name);
        
        OBP_LOG_INFO("[%s] Thread %p global ref count decreased to: %d", 
                    plugin_name.c_str(), &current_thread_id, ref_it->second);
        
        if (ref_it->second <= 0) {
            // Global reference count reached zero, detach thread
            if (attached_threads_.count(current_thread_id) > 0) {
                OBP_LOG_INFO("[%s] Thread %p detaching from JVM", plugin_name.c_str(), &current_thread_id);
                jvm->DetachCurrentThread();
                attached_threads_.erase(current_thread_id);
            }
            global_thread_ref_count_.erase(ref_it);
            thread_plugin_map_.erase(current_thread_id);
        }
    } else {
        OBP_LOG_WARN("[%s] Thread %p was not found in global reference count", 
                    plugin_name.c_str(), &current_thread_id);
    }
}

int GlobalThreadManager::get_thread_ref_count(std::thread::id tid) {
    std::lock_guard<std::mutex> lock(thread_mutex_);
    auto it = global_thread_ref_count_.find(tid);
    return (it != global_thread_ref_count_.end()) ? it->second : 0;
}

int GlobalThreadManager::get_attached_thread_count() {
    std::lock_guard<std::mutex> lock(thread_mutex_);
    return static_cast<int>(attached_threads_.size());
}

ScopedJNIEnvironment::ScopedJNIEnvironment(const std::string& plugin_name, 
                                          const std::string& classpath,
                                          size_t max_heap_mb,
                                          size_t init_heap_mb) 
    : env_(nullptr), plugin_name_(plugin_name), is_valid_(false) {
    
    OBP_LOG_INFO("[%s] ScopedJNIEnvironment constructor called", plugin_name.c_str());
    
    JavaVM* jvm = nullptr;
    
    if (!classpath.empty()) {
        // Create or get JVM with specified classpath
        OBP_LOG_INFO("[%s] Creating/getting JVM with classpath", plugin_name.c_str());
        jvm = GlobalJVMManager::get_or_create_jvm(classpath, max_heap_mb, init_heap_mb);
    } else {
        // Use existing JVM
        OBP_LOG_INFO("[%s] Using existing JVM", plugin_name.c_str());
        jvm = GlobalJVMManager::get_jvm();
    }
    
    if (jvm) {
        OBP_LOG_INFO("[%s] Acquiring JNI environment", plugin_name.c_str());
        env_ = GlobalThreadManager::acquire_jni_env_for_plugin(jvm, plugin_name);
        is_valid_ = (env_ != nullptr);
        OBP_LOG_INFO("[%s] ScopedJNIEnvironment %s", plugin_name.c_str(), is_valid_ ? "SUCCESS" : "FAILED");
    } else {
        OBP_LOG_ERROR("[%s] JVM is null, cannot acquire JNI environment", plugin_name.c_str());
    }
}

ScopedJNIEnvironment::~ScopedJNIEnvironment() {
    OBP_LOG_INFO("[%s] ScopedJNIEnvironment destructor called", plugin_name_.c_str());
    if (env_) {
        JavaVM* jvm = GlobalJVMManager::get_jvm();
        if (jvm) {
            OBP_LOG_INFO("[%s] Releasing JNI environment", plugin_name_.c_str());
            GlobalThreadManager::release_jni_env_for_plugin(jvm, plugin_name_);
        }
    }
    OBP_LOG_INFO("[%s] ScopedJNIEnvironment destructor completed", plugin_name_.c_str());
}

jstring JNIUtils::cpp_string_to_jstring(JNIEnv* env, const std::string& str) {
    if (!env) {
        return nullptr;
    }
    
    jstring jstr = env->NewStringUTF(str.c_str());
    std::string error_msg;
    if (check_and_handle_exception(env, error_msg)) {
        OBP_LOG_ERROR("Failed to create Java string: %s", error_msg.c_str());
        return nullptr;
    }
    
    return jstr;
}

std::string JNIUtils::jstring_to_cpp_string(JNIEnv* env, jstring jstr) {
    if (!env || !jstr) {
        return std::string();
    }
    
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (!chars) {
        std::string error_msg;
        check_and_handle_exception(env, error_msg);
        return std::string();
    }
    
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    
    return result;
}

int JNIUtils::jstring_array_to_cpp_vector(JNIEnv* env, jobjectArray jarray, 
                                         std::vector<std::string>& result) {
    if (!env || !jarray) {
        return -1;
    }
    
    jsize length = env->GetArrayLength(jarray);
    std::string error_msg;
    if (check_and_handle_exception(env, error_msg)) {
        OBP_LOG_ERROR("Failed to get array length: %s", error_msg.c_str());
        return -1;
    }
    
    result.clear();
    result.reserve(length);
    
    // Process array in batches to prevent local reference accumulation
    const jsize BATCH_SIZE = 32;
    
    for (jsize start = 0; start < length; start += BATCH_SIZE) {
        if (env->PushLocalFrame(BATCH_SIZE) < 0) {
            OBP_LOG_ERROR("Failed to push JNI local reference frame");
            return -1;
        }
        
        jsize end = (start + BATCH_SIZE < length) ? start + BATCH_SIZE : length;
        
        for (jsize i = start; i < end; i++) {
            jstring jstr = (jstring)env->GetObjectArrayElement(jarray, i);
            if (check_and_handle_exception(env, error_msg)) {
                OBP_LOG_ERROR("Failed to get array element %d: %s", i, error_msg.c_str());
                env->PopLocalFrame(nullptr);
                return -1;
            }
            
            if (jstr) {
                std::string str = jstring_to_cpp_string(env, jstr);
                result.push_back(str);
            }
        }
        
        env->PopLocalFrame(nullptr);
    }
    
    return 0;
}

bool JNIUtils::check_and_handle_exception(JNIEnv* env, std::string& error_message) {
    if (!env || !env->ExceptionCheck()) {
        return false;
    }
    
    // Get exception object
    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();
    
    if (exception) {
        // Get exception class and toString method
        jclass throwable_class = env->GetObjectClass(exception);
        jmethodID to_string_method = env->GetMethodID(throwable_class, "toString", "()Ljava/lang/String;");
        
        if (to_string_method) {
            jstring exception_string = (jstring)env->CallObjectMethod(exception, to_string_method);
            if (exception_string) {
                error_message = jstring_to_cpp_string(env, exception_string);
                OBP_LOG_WARN("Java exception occurred: %s", error_message.c_str());
            }
        }
        
        // Clean up local references
        env->DeleteLocalRef(throwable_class);
        env->DeleteLocalRef(exception);
    }
    
    return true;
}

std::string JNIUtils::get_class_name(JNIEnv* env, jclass clazz) {
    if (!env || !clazz) {
        return "";
    }
    
    jclass class_class = env->FindClass("java/lang/Class");
    jmethodID get_name_method = env->GetMethodID(class_class, "getName", "()Ljava/lang/String;");
    
    if (get_name_method) {
        jstring class_name_jstr = (jstring)env->CallObjectMethod(clazz, get_name_method);
        if (class_name_jstr) {
            return jstring_to_cpp_string(env, class_name_jstr);
        }
    }
    
    return "";
}

} // namespace jni
} // namespace oceanbase
