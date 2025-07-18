#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../ast/ast.hpp"
#include "qasm_simulator.hpp"

namespace bloch {

    struct Value {
        enum class Type { Int, Float, Bit, Qubit, Void };
        Type type = Type::Void;
        int intValue = 0;
        double floatValue = 0.0;
        int bitValue = 0;
        int qubit = -1;
    };

    class RuntimeEvaluator {
       public:
        void execute(Program& program);
        const std::unordered_map<const Expression*, int>& measurements() const {
            return m_measurements;
        }
        std::string getQasm() const { return m_sim.getQasm(); }

       private:
        QasmSimulator m_sim;
        std::unordered_map<std::string, FunctionDeclaration*> m_functions;
        std::vector<std::unordered_map<std::string, Value>> m_env;
        Value m_returnValue;
        bool m_hasReturn = false;
        std::unordered_map<const Expression*, int> m_measurements;
        struct QubitInfo {
            std::string name;
            bool measured;
        };
        std::vector<QubitInfo> m_qubits;

        Value eval(Expression* expr);
        void exec(Statement* stmt);
        Value call(FunctionDeclaration* fn, const std::vector<Value>& args);
        Value lookup(const std::string& name);
        void assign(const std::string& name, const Value& v);
        int allocateTrackedQubit(const std::string& name);
        void markMeasured(int index);
        void warnUnmeasured() const;
    };

}