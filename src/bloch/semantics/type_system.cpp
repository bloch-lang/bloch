#include "type_system.hpp"

namespace bloch {

    ValueType typeFromString(const std::string& name) {
        if (name == "int")
            return ValueType::Int;
        if (name == "float")
            return ValueType::Float;
        if (name == "string")
            return ValueType::String;
        if (name == "char")
            return ValueType::Char;
        if (name == "qubit")
            return ValueType::Qubit;
        if (name == "bit")
            return ValueType::Bit;
        if (name == "void")
            return ValueType::Void;
        return ValueType::Custom;
    }

    std::string typeToString(ValueType type) {
        switch (type) {
            case ValueType::Int:
                return "int";
            case ValueType::Float:
                return "float";
            case ValueType::String:
                return "string";
            case ValueType::Char:
                return "char";
            case ValueType::Qubit:
                return "qubit";
            case ValueType::Bit:
                return "bit";
            case ValueType::Void:
                return "void";
            case ValueType::Custom:
                return "custom";
            default:
                return "unknown";
        }
    }

    void SymbolTable::beginScope() { m_scopes.emplace_back(); }

    void SymbolTable::endScope() { m_scopes.pop_back(); }

    void SymbolTable::declare(const std::string& name, bool isFinal, ValueType type,
                              const std::string& customName) {
        if (m_scopes.empty())
            return;
        m_scopes.back()[name] = SymbolInfo{isFinal, type, customName};
    }

    bool SymbolTable::isDeclared(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            if (it->count(name))
                return true;
        }
        return false;
    }

    bool SymbolTable::isFinal(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.isFinal;
        }
        return false;
    }

    ValueType SymbolTable::getType(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.type;
        }
        return ValueType::Unknown;
    }

}  // namespace bloch