/**
 * @file   json_processor.cpp
 * @brief  Source file for serialization and deserialization of json
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Sergey Zamaro
 */

#include "json_processor.hpp"

#include <cctype>
#include <format>
#include <string>
#include <string_view>
#include <immintrin.h>
#include <cstring>

#include "dependency_injector.hpp"

namespace Alpha {
    bool avx2Memeq(const char* s1, const char* s2, size_t len) {
        size_t i = 0;
        for(; i + 32 <= len; i += 32) {
            __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s1 + i));
            __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s2 + i));
            __m256i cmp = _mm256_cmpeq_epi8(v1, v2);
            uint32_t mask = _mm256_movemask_epi8(cmp);
            if(mask != 0xFFFFFFFF) {
                return false;
            }
        }
        
        return memcmp(s1 + i, s2 + i, len - i) == 0;
    }

    bool avx2StringEqual(const char* s1, size_t len1, const char* s2, size_t len2) {
        if(len1 != len2) return false;
        return avx2Memeq(s1, s2, len1);
    }



    const char* avx2SubstrFind(const char* haystack, size_t haystackLen, const char* needle, size_t needleLen) {
        if(needleLen == 0) return haystack;
        if(needleLen > 32) {
            return nullptr;
        }
        
        const char* end = haystack + haystackLen;
        
        for (const char* pos = haystack; pos <= end - 32; pos += 32) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pos));
            __m256i first = _mm256_set1_epi8(needle[0]);

            __m256i cmp = _mm256_cmpeq_epi8(first, chunk);
            uint32_t mask = _mm256_movemask_epi8(cmp);

            for(size_t i = 1; i < needleLen && mask; ++i) {
                __m256i shifted_chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pos + i));
                __m256i expected = _mm256_set1_epi8(needle[i]);
                __m256i cmpI = _mm256_cmpeq_epi8(expected, shifted_chunk);
                uint32_t maskI = _mm256_movemask_epi8(cmpI);
                mask &= maskI;
            }

            if(mask) {
                int offset = __builtin_ctz(mask);
                return pos + offset;
            }
        }

        for(const char* pos = haystack; pos <= end - needleLen; ++pos) {
            bool found = true;
            for(size_t i = 0; i < needleLen; ++i) {
                if(pos[i] != needle[i]) {
                    found = false;
                    break;
                }
            }
            if(found) {
                return pos;
            }
        }

        return nullptr;
    }

    std::string_view trimTwoSided(std::string_view sv) {
        sv.remove_prefix(std::min(sv.find_first_not_of(" \t\n\r\f\v"), sv.size()));
        
        sv.remove_suffix(std::min(sv.find_last_not_of(" \t\n\r\f\v") + 1, sv.size()));
        
        return sv;
    }

    std::string_view trimLeft(std::string_view sv, size_t pos = 0) {
        sv.remove_prefix(std::min(sv.find_first_not_of(" \t\n\r\f\v", pos), sv.size()));
        
        return sv;
    }

    std::string_view trimRight(std::string_view sv, size_t pos = 0) {
        sv.remove_suffix(std::min(sv.find_last_not_of(" \t\n\r\f\v", pos) + 1, sv.size()));
        
        return sv;
    }

    JsonObject JsonValue::toObject() {
    }

    JsonArray JsonValue::toArray() {

    }

    void JsonValue::parse() {
        
    }

    void JsonProcessor::popState() {
        if (_stateStack.size() == 0) {
            _currentState = State::Root;
            return;
        }
        _currentState = _stateStack.back();
        _stateStack.pop_back();
    }

    void JsonProcessor::pushKey(std::string_view key) {
        if (_currentKey != "") {
            _keyStack.push_back(_currentKey);
        }
        _currentKey = key;
    }

    void JsonProcessor::popKey() {
        if (_keyStack.size() == 0) {
            _currentKey = "";
            return;
        }
        _currentKey = _keyStack.back();
        _keyStack.pop_back();
    }

    JsonProcessor* JsonProcessor::getInstance(DependencyInjector* injector) {
        static JsonProcessor instance;
        if(injector) {
            instance._injector = injector;
        }
        return &instance;
    }

    std::string JsonProcessor::serialize(std::string_view type_name, std::string_view id, void* obj) {
        return "";
    }

    template<typename T>
    error JsonProcessor::deserialize(T* obj, std::string_view json) {
        JsonValue jsonValue{.key = "", .value = json, .parent_key = ""};
        constexpr auto ctx = std::meta::access_context::unchecked();
        constexpr auto members = std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, ctx)
        );

        template for(constexpr auto member : members) {
            constexpr std::string_view memberId = std::meta::identifier_of(member);
        }
        // trim(json);

        size_t current_line = 1;
        size_t current_line_pos = 0;
        size_t cursor = 0;

        bool requestFinished = false;

        
        _stateStack.reserve(16);

        auto advanceCursor = [&cursor, &current_line_pos]() {
            cursor++;
            current_line_pos++;
        };

        // auto skipSpaceCharacters = [&cursor, &current_line, &current_line_pos, json]() {
        //     while(cursor < json.size()) {
        //         const char ch = json[cursor];
        //         switch(ch) {
        //             case ' ':
        //             case '\t':
        //             case '\r':
        //             case '\f':
        //             case '\v':
        //                 cursor++;
        //                 continue;
        //             case '\n':
        //                 cursor++;
        //                 current_line++;
        //                 current_line_pos = 0;
        //                 continue;
        //             default: break;
        //         }
        //         break;
        //     }
        // };

        // std::string_view line = std::getline(json);
        for(size_t i = 0; !requestFinished && (i < json.size()); ++i, ++current_line_pos) {
            while(i < json.size()) {
                const char ch = json[i];
                switch(ch) {
                    case ' ':
                    case '\t':
                    case '\r':
                    case '\f':
                    case '\v':
                        i++;
                        continue;
                    case '\n':
                        i++;
                        current_line++;
                        current_line_pos = 0;
                        continue;
                    default: break;
                }
                break;
            }
            char ch = json[i];
            switch(_currentState) {
                case State::Root: {
                    switch(static_cast<SpecialTokens>(ch)) {
                        case SpecialTokens::OpeningBrace: {
                            pushState<State::Object>();
                            break;
                        }
                        case SpecialTokens::OpeningBracket: {
                            pushState<State::Array>();
                            break;
                        }
                        default:
                            return {.string_error = std::format("Parse error. Parsing Root. Expected '{}', '{}'. Received '{}' in line {} at position {}", 
                                static_cast<char>(SpecialTokens::OpeningBrace),
                                static_cast<char>(SpecialTokens::OpeningBracket),
                                ch,
                                current_line,
                                current_line_pos
                            )};
                    }
                    break;
                }
                case State::Key: {
                    size_t nameStart = i;
                    char doubleQuotes = static_cast<char>(SpecialTokens::DoubleQuotes);
                    while ((i < json.size()) && (json[i] != doubleQuotes)) { i++; }
                    ch = json[i];
                    if (ch == doubleQuotes) {
                        std::string_view key = json.substr(nameStart, i);
                        pushKey(key);
                        popState();
                        pushState<State::Colon>();
                        break;
                    }
                    else {
                        return {.string_error = std::format("Parse error. Parsing Key. Expected '{}'. Received '{}' in line {} at position {}", 
                            doubleQuotes,
                            ch,
                            current_line,
                            current_line_pos
                        )};
                    }
                    break;
                }
                case State::Colon: {
                    switch(static_cast<SpecialTokens>(ch)) {
                        case SpecialTokens::Colon: {
                            popState();
                            pushState<State::Value>();
                            break;
                        }
                        default: {
                            return {.string_error = std::format("Parse error. Parsing Colon. Expected '{}'. Received '{}' in line {} at position {}", 
                                static_cast<char>(SpecialTokens::Colon),
                                ch,
                                current_line,
                                current_line_pos
                            )};
                            break;
                        }
                    }
                    break;
                }
                case State::Value: {
                    if (ch == static_cast<char>(SpecialTokens::DoubleQuotes)) {
                        popState();
                        pushState<State::String>();
                    }
                    else if (ch == static_cast<char>(SpecialTokens::OpeningBrace)) {
                        popState();
                        pushState<State::Object>();
                    }
                    else if (ch == static_cast<char>(SpecialTokens::OpeningBracket)) {
                        popState();
                        pushState<State::Array>();
                    }
                    else if ((ch == 't') && /* (json.substr(i, i + 4) == "true") */ avx2Memeq(json.data() + i, "true", sizeof("true"))) {
                        i += 3;
                    }
                    else if ((ch == 'f') && /* (json.substr(i, i + 5) == "false") */avx2Memeq(json.data() + i, "false", sizeof("false"))) {
                        i += 4;
                    }
                    else if ((ch == 'n') && /*(json.substr(i, i + 4) == "null")*/avx2Memeq(json.data() + i, "null", sizeof("true"))) {
                        i += 3;
                        //avx2SubstrFind(json.data() + i, 4, "null", 0);
                        
                    }
                    else {
                        return {.string_error = std::format("Parse error. Parsing Value. Unexpected '{}' in line {} at position {}", 
                            ch,
                            current_line,
                            current_line_pos
                        )};
                    }
                    break;
                }
                case State::Object: {
                    switch(static_cast<SpecialTokens>(ch)) {
                        case SpecialTokens::DoubleQuotes: {
                            pushState<State::Key>();
                            break;
                        }
                        default:
                            return {.string_error = std::format("Parse error. Parsing Object. Expected '{}'. Received '{}' in line {} at position {}", 
                                static_cast<char>(SpecialTokens::DoubleQuotes),
                                ch,
                                current_line,
                                current_line_pos
                            )};
                    }
                    break;
                }
                case State::Array: {
                    if(std::isnumber(ch)) {
                        pushState<State::Double>();
                        break;
                    }
                    switch(static_cast<SpecialTokens>(ch)) {
                        case SpecialTokens::OpeningBrace: {
                            pushState<State::Object>();
                            break;
                        }
                        case SpecialTokens::DoubleQuotes: {
                            pushState<State::String>();
                            break;
                        }
                        case SpecialTokens::Comma: {
                            pushState<State::Value>();
                            break;
                        }
                        case SpecialTokens::Minus: {
                            pushState<State::Double>();
                            break;
                        }
                        default:
                            return {.string_error = std::format("Parse error. Parsing Array. Expected '{}'. Received '{}' in line {} at position {}", 
                                static_cast<char>(SpecialTokens::DoubleQuotes),
                                ch,
                                current_line,
                                current_line_pos
                            )};
                    }
                    break;
                }
                case State::Double: {
                    while (i < json.size()) {
                        char keyCh = json[i];
                        if(keyCh >= '0' && keyCh <= '9') {
                            i++;
                            continue;
                        }
                        else if(keyCh == '.') {
                            i++;
                            continue;
                        }
                    }
                    break;
                }
                case State::String: {
                    char keyCh = ch;
                    while (i < json.size()) {
                        keyCh = json[i];
                        if (keyCh == static_cast<char>(SpecialTokens::DoubleQuotes)) {
                            // TODO: read string
                            break;
                        }
                    }
                    if (keyCh != static_cast<char>(SpecialTokens::DoubleQuotes)) {

                    }
                    break;
                }
            } 
        }
        // _injector->get_field_matches<T, >(T *obj, std::string_view fieldName);
    }

}