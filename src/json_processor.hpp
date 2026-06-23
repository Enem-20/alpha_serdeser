/**
 * @file   json_processor.hpp
 * @brief  Header for serialization and deserialization of json
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Sergey Zamaro
 */

#ifndef C_JSON_PROCESSOR_HPP
#define C_JSON_PROCESSOR_HPP

#include "dependency_injector.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class DependencyInjector;

namespace Alpha {

    struct JsonObject;
    struct JsonArray;

    struct error {
        std::string string_error;
    };

    struct JsonValue {
        //raw value
        std::string_view key;
        std::string_view value;
        std::string_view parent_key;

        JsonObject toObject();
        JsonArray toArray();

        void parse();
    };

    struct JsonObject : public JsonValue {
        //parsed value to [key, value]
        std::unordered_map<std::string_view, JsonValue> children;

    };

    struct JsonArray : public JsonValue {
        //parsed value to [index, value]
        std::vector<JsonValue> children;
    };

    class JsonProcessor {
    public:
        enum class SpecialTokens : char {
            OpeningBrace    =   '{',
            ClosingBrace    =   '}',
            OpeningBracket  =   '[',
            ClosingBracket  =   ']',
            Colon           =   ':',
            Comma           =   ',',
            DoubleQuotes    =   '\"',
            Minus           =   '-'
        };

        enum class State : uint32_t {
            Root,
            Key,
            Colon,
            Value,
            Object = int32_t(SpecialTokens::OpeningBrace) + int32_t(SpecialTokens::ClosingBrace),
            Array,
            Double,
            String,
            Bool,
            Null
        };


    private:
        std::vector<State> _stateStack;
        State _currentState = State::Root;
        std::vector<std::string_view> _keyStack;
        std::string_view _currentKey = "";
        DependencyInjector* _injector = nullptr;

        JsonProcessor() = default;

        template<typename T>
        JsonObject getObject(const std::string& object);
        template<State newState>
        void pushState() {
            _stateStack.push_back(_currentState);
            _currentState = newState;
        }

        void popState();

        void pushKey(std::string_view key);
        void popKey();

    public:
        static JsonProcessor* getInstance(DependencyInjector* injector = nullptr);

        std::string serialize(std::string_view type_name, std::string_view id, void* obj);
        template<typename T>
        error deserialize(T* obj, std::string_view json);
    };

}

#endif // C_JSON_PROCESSOR_HPP