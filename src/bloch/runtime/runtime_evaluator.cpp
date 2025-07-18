#include "runtime_evaluator.hpp"
#include "../error/bloch_runtime_error.hpp"
#include "../semantics/built_ins.hpp"

namespace bloch {

    void RuntimeEvaluator::execute(Program& program) {
        for (auto& fn : program.functions) {
            m_functions[fn->name] = fn.get();
        }
        // assume main exists
        auto it = m_functions.find("main");
        if (it != m_functions.end()) {
            call(it->second, {});
        }
    }

    Value RuntimeEvaluator::lookup(const std::string& name) {
        for (auto it = m_env.rbegin(); it != m_env.rend(); ++it) {
            auto fit = it->find(name);
            if (fit != it->end())
                return fit->second;
        }
        return {};
    }

    void RuntimeEvaluator::assign(const std::string& name, const Value& v) {
        for (auto it = m_env.rbegin(); it != m_env.rend(); ++it) {
            auto fit = it->find(name);
            if (fit != it->end()) {
                fit->second = v;
                return;
            }
        }
        m_env.back()[name] = v;
    }

    Value RuntimeEvaluator::call(FunctionDeclaration* fn, const std::vector<Value>& args) {
        m_env.push_back({});
        for (size_t i = 0; i < fn->params.size() && i < args.size(); ++i) {
            m_env.back()[fn->params[i]->name] = args[i];
        }
        m_hasReturn = false;
        if (fn->body)
            for (auto& stmt : fn->body->statements) {
                exec(stmt.get());
                if (m_hasReturn)
                    break;
            }
        Value ret = m_returnValue;
        m_env.pop_back();
        return ret;
    }

    void RuntimeEvaluator::exec(Statement* s) {
        if (!s)
            return;
        if (auto var = dynamic_cast<VariableDeclaration*>(s)) {
            Value v;
            if (auto prim = dynamic_cast<PrimitiveType*>(var->varType.get())) {
                if (prim->name == "int")
                    v.type = Value::Type::Int;
                else if (prim->name == "bit")
                    v.type = Value::Type::Bit;
                else if (prim->name == "float")
                    v.type = Value::Type::Float;
                else if (prim->name == "qubit") {
                    v.type = Value::Type::Qubit;
                    v.qubit = allocateTrackedQubit(var->name);
                }
            }
            if (var->initializer)
                v = eval(var->initializer.get());
            m_env.back()[var->name] = v;
        } else if (auto block = dynamic_cast<BlockStatement*>(s)) {
            m_env.push_back({});
            for (auto& st : block->statements) {
                exec(st.get());
                if (m_hasReturn)
                    break;
            }
            m_env.pop_back();
        } else if (auto exprs = dynamic_cast<ExpressionStatement*>(s)) {
            eval(exprs->expression.get());
        } else if (auto ret = dynamic_cast<ReturnStatement*>(s)) {
            if (ret->value)
                m_returnValue = eval(ret->value.get());
            m_hasReturn = true;
        } else if (auto ifs = dynamic_cast<IfStatement*>(s)) {
            Value cond = eval(ifs->condition.get());
            if (cond.intValue || cond.bitValue) {
                exec(ifs->thenBranch.get());
            } else {
                exec(ifs->elseBranch.get());
            }
        } else if (auto fors = dynamic_cast<ForStatement*>(s)) {
            m_env.push_back({});
            if (fors->initializer)
                exec(fors->initializer.get());
            while (true) {
                Value c = {Value::Type::Bit, 0, 0.0, 0};
                if (fors->condition)
                    c = eval(fors->condition.get());
                if (!(c.intValue || c.bitValue))
                    break;
                exec(fors->body.get());
                if (m_hasReturn)
                    break;
                if (fors->increment)
                    eval(fors->increment.get());
            }
            m_env.pop_back();
        } else if (auto echo = dynamic_cast<EchoStatement*>(s)) {
            Value v = eval(echo->value.get());
            std::cout << (v.type == Value::Type::Int ? std::to_string(v.intValue)
                                                     : std::to_string(v.bitValue))
                      << std::endl;
        } else if (auto reset = dynamic_cast<ResetStatement*>(s)) {
            // ignore
        } else if (auto meas = dynamic_cast<MeasureStatement*>(s)) {
            Value q = eval(meas->qubit.get());
            m_sim.measure(q.qubit);
            markMeasured(q.qubit);
        } else if (auto assignStmt = dynamic_cast<AssignmentStatement*>(s)) {
            Value val = eval(assignStmt->value.get());
            assign(assignStmt->name, val);
        }
    }

    Value RuntimeEvaluator::eval(Expression* e) {
        if (!e)
            return {};
        if (auto lit = dynamic_cast<LiteralExpression*>(e)) {
            Value v;
            v.type = Value::Type::Int;
            v.intValue = std::stoi(lit->value);
            if (lit->literalType == "bit") {
                v.type = Value::Type::Bit;
                v.bitValue = std::stoi(lit->value);
            }
            return v;
        } else if (auto var = dynamic_cast<VariableExpression*>(e)) {
            return lookup(var->name);
        } else if (auto bin = dynamic_cast<BinaryExpression*>(e)) {
            Value l = eval(bin->left.get());
            Value r = eval(bin->right.get());
            if (bin->op == "+") {
                return {Value::Type::Int, l.intValue + r.intValue};
            }
            if (bin->op == "-") {
                return {Value::Type::Int, l.intValue - r.intValue};
            }
            if (bin->op == "*") {
                return {Value::Type::Int, l.intValue * r.intValue};
            }
            if (bin->op == "/") {
                return {Value::Type::Int, l.intValue / r.intValue};
            }
            if (bin->op == "%") {
                return {Value::Type::Int, l.intValue % r.intValue};
            }
            if (bin->op == ">") {
                return {Value::Type::Bit, 0, 0.0, l.intValue > r.intValue};
            }
            if (bin->op == "<") {
                return {Value::Type::Bit, 0, 0.0, l.intValue < r.intValue};
            }
            if (bin->op == ">=") {
                return {Value::Type::Bit, 0, 0.0, l.intValue >= r.intValue};
            }
            if (bin->op == "<=") {
                return {Value::Type::Bit, 0, 0.0, l.intValue <= r.intValue};
            }
            if (bin->op == "==") {
                return {Value::Type::Bit, 0, 0.0, l.intValue == r.intValue};
            }
            if (bin->op == "!=") {
                return {Value::Type::Bit, 0, 0.0, l.intValue != r.intValue};
            }
        } else if (auto unary = dynamic_cast<UnaryExpression*>(e)) {
            Value r = eval(unary->right.get());
            if (unary->op == "-") {
                return {Value::Type::Int, -r.intValue};
            }
            return r;
        } else if (auto callExpr = dynamic_cast<CallExpression*>(e)) {
            if (auto var = dynamic_cast<VariableExpression*>(callExpr->callee.get())) {
                auto name = var->name;
                auto builtin = builtInGates.find(name);
                std::vector<Value> args;
                for (auto& a : callExpr->arguments) args.push_back(eval(a.get()));
                if (builtin != builtInGates.end()) {
                    if (name == "h")
                        m_sim.h(args[0].qubit);
                    else if (name == "x")
                        m_sim.x(args[0].qubit);
                    else if (name == "y")
                        m_sim.y(args[0].qubit);
                    else if (name == "z")
                        m_sim.z(args[0].qubit);
                    else if (name == "rx")
                        m_sim.rx(args[0].qubit, args[1].floatValue);
                    else if (name == "ry")
                        m_sim.ry(args[0].qubit, args[1].floatValue);
                    else if (name == "rz")
                        m_sim.rz(args[0].qubit, args[1].floatValue);
                    else if (name == "cx")
                        m_sim.cx(args[0].qubit, args[1].qubit);
                    return {};  // void
                }
                auto fit = m_functions.find(name);
                if (fit != m_functions.end()) {
                    auto res = call(fit->second, args);
                    if (fit->second->hasQuantumAnnotation && res.type == Value::Type::Bit) {
                        m_measurements[e] = res.bitValue;
                    }
                    return res;
                }
            }
        } else if (auto idx = dynamic_cast<MeasureExpression*>(e)) {
            Value q = eval(idx->qubit.get());
            int bit = m_sim.measure(q.qubit);
            markMeasured(q.qubit);
            m_measurements[e] = bit;
            return {Value::Type::Bit, 0, 0.0, bit};
        } else if (auto assignExpr = dynamic_cast<AssignmentExpression*>(e)) {
            Value v = eval(assignExpr->value.get());
            assign(assignExpr->name, v);
            return v;
        }
        return {};
    }

    int RuntimeEvaluator::allocateTrackedQubit(const std::string& name) {
        int idx = m_sim.allocateQubit();
        m_qubits.push_back({name, false});
        return idx;
    }

    void RuntimeEvaluator::markMeasured(int index) {
        if (index >= 0 && index < static_cast<int>(m_qubits.size()))
            m_qubits[index].measured = true;
    }

    void RuntimeEvaluator::warnUnmeasured() const {
        for (const auto& q : m_qubits) {
            if (!q.measured) {
                std::cerr << "Warning: Qubit " << q.name
                          << " was left unmeasured. No classical value will be returned.\n";
            }
        }
    }

}