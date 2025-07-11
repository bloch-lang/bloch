#include "semantic_analyser.hpp"
#include "built_ins.hpp"

namespace bloch {

    void SemanticAnalyser::analyse(Program& program) {
        beginScope();
        program.accept(*this);
        endScope();
    }

    void SemanticAnalyser::visit(ImportStatement&) {}

    void SemanticAnalyser::visit(VariableDeclaration& node) {
        if (isDeclared(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Variable '" + node.name + "' redeclared");
        }
        std::string typeName;
        if (auto prim = dynamic_cast<PrimitiveType*>(node.varType.get()))
            typeName = prim->name;
        else if (dynamic_cast<VoidType*>(node.varType.get()))
            typeName = "void";
        else if (auto obj = dynamic_cast<ObjectType*>(node.varType.get()))
            typeName = obj->className;
        declare(node.name, node.isFinal, typeName);
        if (node.initializer) {
            if (auto call = dynamic_cast<CallExpression*>(node.initializer.get())) {
                if (auto callee = dynamic_cast<VariableExpression*>(call->callee.get())) {
                    if (returnsVoid(callee->name)) {
                        throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                                "Cannot assign result of void function");
                    }
                }
            }
            node.initializer->accept(*this);
        }
    }

    void SemanticAnalyser::visit(BlockStatement& node) {
        beginScope();
        for (auto& stmt : node.statements) stmt->accept(*this);
        endScope();
    }

    void SemanticAnalyser::visit(ExpressionStatement& node) {
        if (node.expression)
            node.expression->accept(*this);
    }

    void SemanticAnalyser::visit(ReturnStatement& node) {
        bool isVoid = dynamic_cast<VoidType*>(m_currentReturnType) != nullptr;
        if (node.value && isVoid) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Void function cannot return a value");
        }
        if (!node.value && !isVoid) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Non-void function must return a value");
        }
        if (node.value)
            node.value->accept(*this);
    }

    void SemanticAnalyser::visit(IfStatement& node) {
        if (node.condition)
            node.condition->accept(*this);
        if (node.thenBranch)
            node.thenBranch->accept(*this);
        if (node.elseBranch)
            node.elseBranch->accept(*this);
    }

    void SemanticAnalyser::visit(ForStatement& node) {
        beginScope();
        if (node.initializer)
            node.initializer->accept(*this);
        if (node.condition)
            node.condition->accept(*this);
        if (node.increment)
            node.increment->accept(*this);
        if (node.body)
            node.body->accept(*this);
        endScope();
    }

    void SemanticAnalyser::visit(EchoStatement& node) {
        if (node.value)
            node.value->accept(*this);
    }

    void SemanticAnalyser::visit(ResetStatement& node) {
        if (node.target)
            node.target->accept(*this);
    }

    void SemanticAnalyser::visit(MeasureStatement& node) {
        if (node.qubit)
            node.qubit->accept(*this);
    }

    void SemanticAnalyser::visit(AssignmentStatement& node) {
        if (!isDeclared(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Variable '" + node.name + "' not declared");
        }
        if (isFinal(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Cannot assign to final variable '" + node.name + "'");
        }
        if (node.value) {
            if (auto call = dynamic_cast<CallExpression*>(node.value.get())) {
                if (auto callee = dynamic_cast<VariableExpression*>(call->callee.get())) {
                    if (returnsVoid(callee->name)) {
                        throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                                "Cannot assign result of void function");
                    }
                }
            }
            node.value->accept(*this);
        }
    }

    void SemanticAnalyser::visit(BinaryExpression& node) {
        if (node.left)
            node.left->accept(*this);
        if (node.right)
            node.right->accept(*this);
    }

    void SemanticAnalyser::visit(UnaryExpression& node) {
        if (node.right)
            node.right->accept(*this);
    }

    void SemanticAnalyser::visit(LiteralExpression&) {}

    void SemanticAnalyser::visit(VariableExpression& node) {
        if (!isDeclared(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Variable '" + node.name + "' not declared");
        }
    }

    void SemanticAnalyser::visit(CallExpression& node) {
        if (auto var = dynamic_cast<VariableExpression*>(node.callee.get())) {
            if (!isDeclared(var->name) && !isFunctionDeclared(var->name)) {
                throw BlochRuntimeError("Bloch Semantic Error", var->line, var->column,
                                        "Variable '" + var->name + "' not declared");
            }
            size_t expected = getFunctionParamCount(var->name);
            if (expected != node.arguments.size()) {
                throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                        "Function '" + var->name + "' expects " +
                                            std::to_string(expected) + " argument(s)");
            }
            auto types = getFunctionParamTypes(var->name);
            for (size_t i = 0; i < node.arguments.size() && i < types.size(); ++i) {
                auto& arg = node.arguments[i];
                std::string expectedType = types[i];
                if (expectedType.empty())
                    continue;
                if (auto argVar = dynamic_cast<VariableExpression*>(arg.get())) {
                    std::string actual = getVariableType(argVar->name);
                    if (!actual.empty() && actual != expectedType) {
                        throw BlochRuntimeError(
                            "Bloch Semantic Error", argVar->line, argVar->column,
                            "Argument " + std::to_string(i + 1) + " of '" + var->name +
                                "' expects type '" + expectedType + "'");
                    }
                } else if (auto argLit = dynamic_cast<LiteralExpression*>(arg.get())) {
                    std::string actual = argLit->literalType;
                    if (!actual.empty() && actual != expectedType) {
                        throw BlochRuntimeError(
                            "Bloch Semantic Error", argLit->line, argLit->column,
                            "Argument " + std::to_string(i + 1) + " of '" + var->name +
                                "' expects type '" + expectedType + "'");
                    }
                }
            }
        } else if (node.callee) {
            node.callee->accept(*this);
        }
        for (auto& arg : node.arguments) arg->accept(*this);
    }

    void SemanticAnalyser::visit(IndexExpression& node) {
        if (node.collection)
            node.collection->accept(*this);
        if (node.index)
            node.index->accept(*this);
    }

    void SemanticAnalyser::visit(ParenthesizedExpression& node) {
        if (node.expression)
            node.expression->accept(*this);
    }

    void SemanticAnalyser::visit(MeasureExpression& node) {
        if (node.qubit)
            node.qubit->accept(*this);
    }

    void SemanticAnalyser::visit(AssignmentExpression& node) {
        if (!isDeclared(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Variable '" + node.name + "' not declared");
        }
        if (isFinal(node.name)) {
            throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                    "Cannot assign to final variable '" + node.name + "'");
        }
        if (node.value) {
            if (auto call = dynamic_cast<CallExpression*>(node.value.get())) {
                if (auto callee = dynamic_cast<VariableExpression*>(call->callee.get())) {
                    if (returnsVoid(callee->name)) {
                        throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                                "Cannot assign result of void function");
                    }
                }
            }
            node.value->accept(*this);
        }
    }

    void SemanticAnalyser::visit(ConstructorCallExpression& node) {
        for (auto& arg : node.arguments) arg->accept(*this);
    }

    void SemanticAnalyser::visit(MemberAccessExpression& node) {
        if (node.object)
            node.object->accept(*this);
    }

    void SemanticAnalyser::visit(PrimitiveType&) {}
    void SemanticAnalyser::visit(LogicalType&) {}
    void SemanticAnalyser::visit(ArrayType&) {}
    void SemanticAnalyser::visit(VoidType&) {}
    void SemanticAnalyser::visit(ObjectType&) {}

    void SemanticAnalyser::visit(Parameter& node) {
        if (node.type)
            node.type->accept(*this);
    }

    void SemanticAnalyser::visit(AnnotationNode&) {}

    void SemanticAnalyser::visit(FunctionDeclaration& node) {
        if (node.hasQuantumAnnotation) {
            bool valid = false;
            if (dynamic_cast<VoidType*>(node.returnType.get())) {
                valid = true;
            } else if (auto prim = dynamic_cast<PrimitiveType*>(node.returnType.get())) {
                if (prim->name == "bit")
                    valid = true;
            }
            if (!valid) {
                throw BlochRuntimeError("Bloch Semantic Error", node.line, node.column,
                                        "@quantum functions must return 'bit' or 'void'");
            }
        }
        Type* prevReturn = m_currentReturnType;
        m_currentReturnType = node.returnType.get();

        FunctionInfo info;
        info.returnType = node.returnType.get();
        for (auto& param : node.params) {
            if (auto prim = dynamic_cast<PrimitiveType*>(param->type.get()))
                info.paramTypes.push_back(prim->name);
            else if (dynamic_cast<VoidType*>(param->type.get()))
                info.paramTypes.push_back("void");
            else if (auto obj = dynamic_cast<ObjectType*>(param->type.get()))
                info.paramTypes.push_back(obj->className);
            else
                info.paramTypes.push_back("");
        }
        m_functionInfo[node.name] = info;

        beginScope();
        for (auto& param : node.params) {
            if (isDeclared(param->name)) {
                throw BlochRuntimeError("Bloch Semantic Error", param->line, param->column,
                                        "Parameter '" + param->name + "' redeclared");
            }
            std::string typeName;
            if (auto prim = dynamic_cast<PrimitiveType*>(param->type.get()))
                typeName = prim->name;
            else if (dynamic_cast<VoidType*>(param->type.get()))
                typeName = "void";
            else if (auto obj = dynamic_cast<ObjectType*>(param->type.get()))
                typeName = obj->className;
            declare(param->name, false, typeName);
            param->accept(*this);
        }
        if (node.body)
            node.body->accept(*this);
        endScope();

        m_currentReturnType = prevReturn;
    }

    void SemanticAnalyser::visit(ClassDeclaration& node) {
        for (auto& member : node.members) member->accept(*this);
        for (auto& method : node.methods) method->accept(*this);
    }

    void SemanticAnalyser::visit(Program& node) {
        for (auto& fn : node.functions) {
            if (isFunctionDeclared(fn->name)) {
                throw BlochRuntimeError("Bloch Semantic Error", fn->line, fn->column,
                                        "Function '" + fn->name + "' redeclared");
            }
            declareFunction(fn->name);
        }
        for (auto& cls : node.classes) {
            for (auto& method : cls->methods) {
                if (isFunctionDeclared(method->name)) {
                    throw BlochRuntimeError("Bloch Semantic Error", method->line, method->column,
                                            "Function '" + method->name + "' redeclared");
                }
                declareFunction(method->name);
            }
        }
        for (auto& imp : node.imports) imp->accept(*this);
        for (auto& fn : node.functions) fn->accept(*this);
        for (auto& cls : node.classes) cls->accept(*this);
        for (auto& stmt : node.statements) stmt->accept(*this);
    }

    void SemanticAnalyser::beginScope() { m_scopes.emplace_back(); }

    void SemanticAnalyser::endScope() { m_scopes.pop_back(); }

    void SemanticAnalyser::declare(const std::string& name, bool isFinalVar,
                                   const std::string& typeName) {
        if (m_scopes.empty())
            return;
        m_scopes.back()[name] = VariableInfo{isFinalVar, typeName};
    }

    bool SemanticAnalyser::isDeclared(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            if (it->count(name))
                return true;
        }
        return false;
    }

    void SemanticAnalyser::declareFunction(const std::string& name) { m_functions.insert(name); }

    bool SemanticAnalyser::isFunctionDeclared(const std::string& name) const {
        return m_functions.count(name) > 0 || builtInGates.count(name) > 0;
    }

    bool SemanticAnalyser::isFinal(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.isFinal;
        }
        return false;
    }

    size_t SemanticAnalyser::getFunctionParamCount(const std::string& name) const {
        auto it = m_functionInfo.find(name);
        if (it != m_functionInfo.end())
            return it->second.paramTypes.size();
        auto builtin = builtInGates.find(name);
        if (builtin != builtInGates.end())
            return builtin->second.paramTypes.size();
        return 0;
    }

    std::vector<std::string> SemanticAnalyser::getFunctionParamTypes(
        const std::string& name) const {
        auto it = m_functionInfo.find(name);
        if (it != m_functionInfo.end())
            return it->second.paramTypes;
        auto builtin = builtInGates.find(name);
        if (builtin != builtInGates.end())
            return builtin->second.paramTypes;
        return {};
    }

    std::string SemanticAnalyser::getVariableType(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.typeName;
        }
        return "";
    }

    bool SemanticAnalyser::returnsVoid(const std::string& name) const {
        auto it = m_functionInfo.find(name);
        if (it != m_functionInfo.end())
            return dynamic_cast<VoidType*>(it->second.returnType) != nullptr;
        auto builtin = builtInGates.find(name);
        if (builtin != builtInGates.end())
            return builtin->second.returnType == "void";
        return false;
    }

}