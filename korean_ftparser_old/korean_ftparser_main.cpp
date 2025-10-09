/*
 * Copyright (c) 2025 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <new>
#include "oceanbase/ob_plugin_ftparser.h"
#include "korean_parser_core.h"

/**
 * @defgroup KoreanFtParser Korean Fulltext Parser Plugin
 * @brief Korean language fulltext parser plugin with JNI support
 * @{
 */

extern "C" {

/**
 * Korean FTParser scan_begin callback function
 * @param param ObPluginFTParserParamPtr passed by OceanBase
 * @return OBP_SUCCESS on success, error code on failure
 * @details This function is called when OceanBase starts a new fulltext parsing session.
 * It creates a new ObKoreanFTParser instance and initializes it with the provided text.
 */
int korean_ftparser_scan_begin(ObPluginFTParserParamPtr param)
{
    int ret = OBP_SUCCESS;
    
    // Validate input parameter
    if (0 == param) {
        ret = OBP_INVALID_ARGUMENT;
        OBP_LOG_WARN("Invalid parameter: param is null. ret=%d", ret);
        return ret;
    }
    
    // Create FT Parser instance
    oceanbase::korean_ftparser::ObKoreanFTParser *parser = new (std::nothrow) oceanbase::korean_ftparser::ObKoreanFTParser;
    if (nullptr == parser) {
        ret = OBP_ALLOCATE_MEMORY_FAILED;
        OBP_LOG_WARN("Failed to allocate memory for ObKoreanFTParser. ret=%d", ret);
        return ret;
    }
    
    // Initialize the parser with the provided text
    ret = parser->init(param);
    if (OBP_SUCCESS != ret) {
        OBP_LOG_WARN("Failed to initialize Korean FT Parser. ret=%d, parser=%p", ret, parser);
        delete parser;
        return ret;
    }
    
    // Store parser instance in user data for later retrieval
    obp_ftparser_set_user_data(param, parser);
    
    OBP_LOG_INFO("Korean FT Parser scan_begin completed successfully. parser=%p", parser);
    return OBP_SUCCESS;
}

/**
 * Korean FTParser scan_end callback function
 * @param param ObPluginFTParserParamPtr passed by OceanBase
 * @return OBP_SUCCESS on success
 * @details This function is called when OceanBase finishes a fulltext parsing session.
 * It cleans up the ObKoreanFTParser instance and releases resources.
 */
int korean_ftparser_scan_end(ObPluginFTParserParamPtr param)
{
    if (0 == param) {
        OBP_LOG_WARN("Invalid parameter: param is null");
        return OBP_INVALID_ARGUMENT;
    }
    
    // Retrieve parser instance from user data
    oceanbase::korean_ftparser::ObKoreanFTParser *parser = (oceanbase::korean_ftparser::ObKoreanFTParser *)(obp_ftparser_user_data(param));
    if (nullptr != parser) {
        OBP_LOG_INFO("Cleaning up Korean FT Parser. parser=%p", parser);
        delete parser;
    }
    
    // Clear user data
    obp_ftparser_set_user_data(param, 0);
    
    OBP_LOG_INFO("Korean FT Parser scan_end completed successfully");
    return OBP_SUCCESS;
}

/**
 * Korean FTParser next_token callback function
 * @param param ObPluginFTParserParamPtr passed by OceanBase
 * @param word Output parameter for the token word
 * @param word_len Output parameter for the token byte length
 * @param char_cnt Output parameter for the token character count
 * @param word_freq Output parameter for the token frequency
 * @return OBP_SUCCESS on success, OBP_ITER_END when no more tokens, error code on failure
 * @details This function is called repeatedly by OceanBase to get the next token
 * from the Korean text. It delegates to the ObKoreanFTParser instance.
 */
int korean_ftparser_next_token(ObPluginFTParserParamPtr param,
                            char **word,
                            int64_t *word_len,
                            int64_t *char_cnt,
                            int64_t *word_freq)
{
    int ret = OBP_SUCCESS;
    
    // Validate input parameters
    if (0 == param || nullptr == word || nullptr == word_len || 
        nullptr == char_cnt || nullptr == word_freq) {
        ret = OBP_INVALID_ARGUMENT;
        OBP_LOG_WARN("Invalid arguments. ret=%d, param=%p, word=%p, word_len=%p, char_cnt=%p, word_freq=%p", 
                     ret, param, word, word_len, char_cnt, word_freq);
        return ret;
    }
    
    // Retrieve parser instance from user data
    oceanbase::korean_ftparser::ObKoreanFTParser *parser = (oceanbase::korean_ftparser::ObKoreanFTParser *)(obp_ftparser_user_data(param));
    if (nullptr == parser) {
        ret = OBP_PLUGIN_ERROR;
        OBP_LOG_WARN("Parser instance is null. ret=%d", ret);
        return ret;
    }
    
    // Delegate to the parser instance
    ret = parser->get_next_token((const char *&)(*word), *word_len, *char_cnt, *word_freq);
    
    if (OBP_SUCCESS == ret) {
        OBP_LOG_TRACE("Got next token: word=%.*s, word_len=%ld, char_cnt=%ld, word_freq=%ld",
                      (int)*word_len, *word, *word_len, *char_cnt, *word_freq);
    } else if (OBP_ITER_END == ret) {
        OBP_LOG_TRACE("No more tokens available. ret=%d", ret);
    } else {
        OBP_LOG_WARN("Failed to get next token. ret=%d", ret);
    }
    
    return ret;
}

/**
 * Korean FTParser get_add_word_flag callback function
 * @param flag Output parameter for the add word flags
 * @return OBP_SUCCESS on success, error code on failure
 * @details This function returns the flags that control how OceanBase processes
 * the tokens returned by this parser.
 */
int korean_ftparser_get_add_word_flag(uint64_t *flag)
{
    int ret = OBP_SUCCESS;
    
    if (nullptr == flag) {
        ret = OBP_INVALID_ARGUMENT;
        OBP_LOG_WARN("Invalid argument: flag is null. ret=%d", ret);
        return ret;
    }
    
    // Set flags for Korean language processing
    // Disable problematic filters that may reject Korean characters
    *flag = OBP_FTPARSER_AWF_CASEDOWN      // Convert to lowercase (safe for Korean)
            | OBP_FTPARSER_AWF_GROUPBY_WORD; // Group identical words (safe for Korean)
    // Disabled: OBP_FTPARSER_AWF_MIN_MAX_WORD (may filter Korean chars by length)
    // Disabled: OBP_FTPARSER_AWF_STOPWORD (may use inappropriate stopword list)
    
    OBP_LOG_TRACE("Korean FT Parser add word flags: 0x%lx", *flag);
    return ret;
}

/**
 * Plugin initialization function
 * @param plugin The plugin parameter passed by OceanBase
 * @return OBP_SUCCESS on success, error code on failure
 * @details This function is called when OceanBase loads the plugin library.
 * It registers the Korean fulltext parser with OceanBase.
 */
int plugin_init_ko(ObPluginParamPtr plugin)
{
    int ret = OBP_SUCCESS;
    
    if (0 == plugin) {
        ret = OBP_INVALID_ARGUMENT;
        OBP_LOG_WARN("Invalid plugin parameter. ret=%d", ret);
        return ret;
    }
    
    OBP_LOG_INFO("Initializing Korean FT Parser plugin...");

    // Define the ftparser plugin descriptor
    // This structure defines the callback functions that OceanBase will call
    ObPluginFTParser parser = {
        .init              = NULL,                          // No global init needed
        .deinit            = NULL,                          // No global deinit needed
        .scan_begin        = korean_ftparser_scan_begin,      // Called before parsing
        .scan_end          = korean_ftparser_scan_end,        // Called after parsing
        .next_token        = korean_ftparser_next_token,      // Called to get tokens
        .get_add_word_flag = korean_ftparser_get_add_word_flag // Called to get processing flags
    };
    
    // Register the ftparser plugin with OceanBase
    ret = OBP_REGISTER_FTPARSER(plugin,
                                "korean_ftparser",                    // Plugin name
                                parser,                             // Plugin descriptor
                                "Korean language fulltext parser with JNI support"); // Description
    
    if (OBP_SUCCESS == ret) {
        OBP_LOG_INFO("Korean FT Parser plugin registered successfully");
    } else {
        OBP_LOG_WARN("Failed to register Korean FT Parser plugin. ret=%d", ret);
    }
    
    return ret;
}

/**
 * Plugin declaration
 * @details This macro declares the plugin metadata that OceanBase uses
 * to load and manage the plugin.
 */
OBP_DECLARE_PLUGIN(korean_ftparser)
{
    OBP_AUTHOR_OCEANBASE,                    // Plugin author
    OBP_MAKE_VERSION(1, 0, 0),              // Plugin version
    OBP_LICENSE_MULAN_PSL_V2,               // Plugin license
    plugin_init_ko,                            // Plugin initialization function
    nullptr,                                // Plugin deinitialization function (not needed)
} OBP_DECLARE_PLUGIN_END;

} // extern "C"

/** @} */