#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace std;

// ==========================================
// 1. CORE TYPES & FORWARD DECLARATIONS
// ==========================================

enum ValueType
{
    V_NULL,
    V_NUM,
    V_STR,
    V_BOOL,
    V_LIST,
    V_OBJ,
    V_FUNC,
    V_NATIVE
};

struct Value;
struct ASTNode;
class Environment;

// A "Box" to hold our dynamic values safely
struct Value
{
    ValueType type = V_NULL;
    double numVal = 0;
    string strVal;
    bool boolVal = false;
    vector<shared_ptr<Value>> listVal;
    map<string, shared_ptr<Value>> objVal;

    // For User Functions
    vector<string> params;
    shared_ptr<ASTNode> body;        // AST Node for function body
    shared_ptr<Environment> closure; // Closure scope

    // For Native Functions (print, setTimeout)
    function<shared_ptr<Value>(vector<shared_ptr<Value>>)> nativeFn;

    string toString()
    {
        if (type == V_NUM)
        {
            string s = to_string(numVal);
            return s.substr(0, s.find_last_not_of('0') + 1);
        }
        if (type == V_STR)
            return strVal;
        if (type == V_BOOL)
            return boolVal ? "true" : "false";
        if (type == V_NULL)
            return "null";
        if (type == V_LIST)
            return "[Array]";
        if (type == V_OBJ)
            return "[Object]";
        if (type == V_FUNC || type == V_NATIVE)
            return "[Function]";
        return "";
    }
};

// ==========================================
// 2. THE ENVIRONMENT (SCOPE)
// ==========================================

class Environment : public enable_shared_from_this<Environment>
{
public:
    map<string, shared_ptr<Value>> vars;
    shared_ptr<Environment> parent;

    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    void define(string name, shared_ptr<Value> val)
    {
        vars[name] = val;
    }

    shared_ptr<Value> lookup(string name)
    {
        if (vars.count(name))
            return vars[name];
        if (parent)
            return parent->lookup(name);
        throw runtime_error("Undefined variable: " + name);
    }

    void assign(string name, shared_ptr<Value> val)
    {
        if (vars.count(name))
        {
            vars[name] = val;
            return;
        }
        if (parent)
        {
            parent->assign(name, val);
            return;
        }
        throw runtime_error("Cannot assign to undefined variable: " + name);
    }
};

// ==========================================
// 3. ABSTRACT SYNTAX TREE (AST) NODES
// ==========================================

struct ASTNode
{
    virtual ~ASTNode() = default;
    virtual shared_ptr<Value> eval(shared_ptr<Environment> env) = 0;
};

// --- Literals ---
struct NumberNode : ASTNode
{
    double val;
    NumberNode(double v) : val(v) {}
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto v = make_shared<Value>();
        v->type = V_NUM;
        v->numVal = val;
        return v;
    }
};

struct StringNode : ASTNode
{
    string val;
    StringNode(string v) : val(v) {}
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto v = make_shared<Value>();
        v->type = V_STR;
        v->strVal = val;
        return v;
    }
};

struct IdentifierNode : ASTNode
{
    string name;
    IdentifierNode(string n) : name(n) {}
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        return env->lookup(name);
    }
};

// --- Structures ---
struct ArrayNode : ASTNode
{
    vector<shared_ptr<ASTNode>> elements;
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto arr = make_shared<Value>();
        arr->type = V_LIST;
        for (auto &el : elements)
            arr->listVal.push_back(el->eval(env));
        return arr;
    }
};

struct ObjectNode : ASTNode
{
    map<string, shared_ptr<ASTNode>> props;
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto obj = make_shared<Value>();
        obj->type = V_OBJ;
        for (auto const &[key, valNode] : props)
        {
            obj->objVal[key] = valNode->eval(env);
        }
        return obj;
    }
};

// --- Operations ---
struct BinaryOpNode : ASTNode
{
    string op;
    shared_ptr<ASTNode> left, right;
    BinaryOpNode(string o, shared_ptr<ASTNode> l, shared_ptr<ASTNode> r) : op(o), left(l), right(r) {}

    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto l = left->eval(env);
        auto r = right->eval(env);
        auto res = make_shared<Value>();

        if (op == "+")
        {
            if (l->type == V_STR || r->type == V_STR)
            {
                res->type = V_STR;
                res->strVal = l->toString() + r->toString();
            }
            else
            {
                res->type = V_NUM;
                res->numVal = l->numVal + r->numVal;
            }
        }
        else if (op == "-")
        {
            res->type = V_NUM;
            res->numVal = l->numVal - r->numVal;
        }
        else if (op == "*")
        {
            res->type = V_NUM;
            res->numVal = l->numVal * r->numVal;
        }
        else if (op == "/")
        {
            res->type = V_NUM;
            res->numVal = l->numVal / r->numVal;
        }
        else if (op == ">")
        {
            res->type = V_BOOL;
            res->boolVal = l->numVal > r->numVal;
        }
        else if (op == "<")
        {
            res->type = V_BOOL;
            res->boolVal = l->numVal < r->numVal;
        }
        else if (op == "==")
        {
            if (l->type == V_NUM)
                res->boolVal = (l->numVal == r->numVal);
            else if (l->type == V_STR)
                res->boolVal = (l->strVal == r->strVal);
            res->type = V_BOOL;
        }
        return res;
    }
};

// --- Statements ---
struct BlockNode : ASTNode
{
    vector<shared_ptr<ASTNode>> statements;
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        shared_ptr<Value> lastVal = make_shared<Value>();
        for (auto &stmt : statements)
        {
            lastVal = stmt->eval(env);
            // In a real engine, we would handle 'return' signals here
        }
        return lastVal;
    }
};

struct VarDeclNode : ASTNode
{
    string name;
    shared_ptr<ASTNode> init;
    VarDeclNode(string n, shared_ptr<ASTNode> i) : name(n), init(i) {}
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto val = init ? init->eval(env) : make_shared<Value>();
        env->define(name, val);
        return val;
    }
};

struct IfNode : ASTNode
{
    shared_ptr<ASTNode> cond, thenBranch, elseBranch;
    IfNode(shared_ptr<ASTNode> c, shared_ptr<ASTNode> t, shared_ptr<ASTNode> e)
        : cond(c), thenBranch(t), elseBranch(e) {}

    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto c = cond->eval(env);
        bool truthy = false;
        if (c->type == V_BOOL)
            truthy = c->boolVal;
        else if (c->type == V_NUM)
            truthy = (c->numVal != 0);

        if (truthy)
            return thenBranch->eval(env);
        else if (elseBranch)
            return elseBranch->eval(env);
        return make_shared<Value>();
    }
};

struct WhileNode : ASTNode
{
    shared_ptr<ASTNode> cond, body;
    WhileNode(shared_ptr<ASTNode> c, shared_ptr<ASTNode> b) : cond(c), body(b) {}
    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        while (true)
        {
            auto c = cond->eval(env);
            if ((c->type == V_BOOL && !c->boolVal) || (c->type == V_NUM && c->numVal == 0))
                break;
            body->eval(env);
        }
        return make_shared<Value>();
    }
};

struct FunctionDeclNode : ASTNode
{
    string name;
    vector<string> params;
    shared_ptr<ASTNode> body;
    FunctionDeclNode(string n, vector<string> p, shared_ptr<ASTNode> b)
        : name(n), params(p), body(b) {}

    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto func = make_shared<Value>();
        func->type = V_FUNC;
        func->params = params;
        func->body = body;
        func->closure = env; // Capture scope!
        env->define(name, func);
        return func;
    }
};

struct CallNode : ASTNode
{
    string callee;
    vector<shared_ptr<ASTNode>> args;
    CallNode(string c, vector<shared_ptr<ASTNode>> a) : callee(c), args(a) {}

    shared_ptr<Value> eval(shared_ptr<Environment> env) override
    {
        auto func = env->lookup(callee);
        vector<shared_ptr<Value>> argVals;
        for (auto &a : args)
            argVals.push_back(a->eval(env));

        if (func->type == V_NATIVE)
        {
            return func->nativeFn(argVals);
        }

        if (func->type == V_FUNC)
        {
            auto scope = make_shared<Environment>(func->closure);
            for (size_t i = 0; i < func->params.size(); ++i)
            {
                if (i < argVals.size())
                    scope->define(func->params[i], argVals[i]);
            }
            return func->body->eval(scope);
        }
        throw runtime_error("Not a function: " + callee);
    }
};

// ==========================================
// 4. PARSER (TURNS TOKENS -> AST)
// ==========================================

class Parser
{
    string src;
    size_t pos = 0;

public:
    Parser(string s) : src(s) {}

    char peek(int offset = 0)
    {
        if (pos + offset >= src.size())
            return 0;
        return src[pos + offset];
    }

    char advance() { return src[pos++]; }

    void skipWhitespace()
    {
        while (pos < src.size() && isspace(src[pos]))
            pos++;
    }

    string parseToken()
    {
        skipWhitespace();
        if (pos >= src.size())
            return "";

        char c = peek();
        if (isalpha(c))
        {
            string s;
            while (isalnum(peek()))
                s += advance();
            return s;
        }
        if (isdigit(c))
        {
            string s;
            while (isdigit(peek()) || peek() == '.')
                s += advance();
            return s;
        }
        if (c == '"')
        {
            advance();
            string s;
            while (peek() != '"')
                s += advance();
            advance();
            return "\"" + s + "\"";
        }
        string op;
        op += advance();
        if ((op == "=" || op == "!" || op == "<" || op == ">") && peek() == '=')
            op += advance();
        return op;
    }

    // --- Recursive Descent ---

    shared_ptr<ASTNode> parseExpression()
    {
        return parseComparison();
    }

    shared_ptr<ASTNode> parseComparison()
    {
        auto left = parseAdditive();
        skipWhitespace();
        char c = peek();
        if (c == '>' || c == '<' || c == '=')
        {
            string op = parseToken();
            auto right = parseAdditive();
            return make_shared<BinaryOpNode>(op, left, right);
        }
        return left;
    }

    shared_ptr<ASTNode> parseAdditive()
    {
        auto left = parseMultiplicative();
        while (true)
        {
            skipWhitespace();
            char c = peek();
            if (c != '+' && c != '-')
                break;
            string op = parseToken();
            auto right = parseMultiplicative();
            left = make_shared<BinaryOpNode>(op, left, right);
        }
        return left;
    }

    shared_ptr<ASTNode> parseMultiplicative()
    {
        auto left = parsePrimary();
        while (true)
        {
            skipWhitespace();
            char c = peek();
            if (c != '*' && c != '/')
                break;
            string op = parseToken();
            auto right = parsePrimary();
            left = make_shared<BinaryOpNode>(op, left, right);
        }
        return left;
    }

    shared_ptr<ASTNode> parsePrimary()
    {
        skipWhitespace();
        char c = peek();

        if (isdigit(c))
            return make_shared<NumberNode>(stod(parseToken()));
        if (c == '"')
        {
            string s = parseToken();
            return make_shared<StringNode>(s.substr(1, s.length() - 2));
        }
        if (c == '[')
        {
            advance(); // [
            auto arr = make_shared<ArrayNode>();
            while (peek() != ']')
            {
                arr->elements.push_back(parseExpression());
                skipWhitespace();
                if (peek() == ',')
                    advance();
            }
            advance(); // ]
            return arr;
        }
        if (isalpha(c))
        {
            string name = parseToken();
            skipWhitespace();
            if (peek() == '(')
            {              // Function Call
                advance(); // (
                vector<shared_ptr<ASTNode>> args;
                while (peek() != ')')
                {
                    args.push_back(parseExpression());
                    skipWhitespace();
                    if (peek() == ',')
                        advance();
                }
                advance(); // )
                return make_shared<CallNode>(name, args);
            }
            return make_shared<IdentifierNode>(name);
        }

        return nullptr; // Error or unsupported
    }

    shared_ptr<ASTNode> parseBlock()
    {
        advance(); // {
        auto block = make_shared<BlockNode>();
        while (peek() != '}' && pos < src.size())
        {
            block->statements.push_back(parseStatement());
            skipWhitespace();
        }
        advance(); // }
        return block;
    }

    shared_ptr<ASTNode> parseStatement()
    {
        skipWhitespace();
        string tokenStr;
        size_t checkpoint = pos;

        // Peek next word
        if (isalpha(peek()))
        {
            while (isalpha(peek()))
                tokenStr += advance();
        }

        if (tokenStr == "var")
        {
            skipWhitespace();
            string name = parseToken();
            skipWhitespace();
            char eq = advance(); // =
            auto val = parseExpression();
            skipWhitespace();
            if (peek() == ';')
                advance();
            return make_shared<VarDeclNode>(name, val);
        }
        else if (tokenStr == "if")
        {
            skipWhitespace();
            advance(); // (
            auto cond = parseExpression();
            advance(); // )
            skipWhitespace();
            auto thenB = parseBlock(); // Assumes { }
            shared_ptr<ASTNode> elseB = nullptr;
            skipWhitespace();
            // simple hack to peek 'else'
            size_t p2 = pos;
            string nxt;
            while (isalpha(src[p2]))
                nxt += src[p2++];
            if (nxt == "else")
            {
                pos = p2;
                skipWhitespace();
                elseB = parseBlock();
            }
            return make_shared<IfNode>(cond, thenB, elseB);
        }
        else if (tokenStr == "while")
        {
            skipWhitespace();
            advance(); // (
            auto cond = parseExpression();
            advance(); // )
            skipWhitespace();
            auto body = parseBlock();
            return make_shared<WhileNode>(cond, body);
        }
        else if (tokenStr == "function")
        {
            skipWhitespace();
            string name = parseToken();
            advance(); // (
            vector<string> params;
            while (peek() != ')')
            {
                skipWhitespace();
                string p;
                while (isalnum(peek()))
                    p += advance();
                params.push_back(p);
                skipWhitespace();
                if (peek() == ',')
                    advance();
            }
            advance(); // )
            skipWhitespace();
            auto body = parseBlock();
            return make_shared<FunctionDeclNode>(name, params, body);
        }

        // Reset if not a keyword, it's an expression or assignment
        pos = checkpoint;
        return parseExpression();
    }

    vector<shared_ptr<ASTNode>> parse()
    {
        vector<shared_ptr<ASTNode>> stmts;
        while (pos < src.size())
        {
            skipWhitespace();
            if (pos >= src.size())
                break;
            stmts.push_back(parseStatement());
            skipWhitespace();
            if (peek() == ';')
                advance();
        }
        return stmts;
    }
};

// ==========================================
// 5. EVENT LOOP (ASYNC SIMULATION)
// ==========================================

struct Task
{
    long long executeTime;
    function<void()> callback;

    // Min-heap comparator (smallest time first)
    bool operator>(const Task &other) const
    {
        return executeTime > other.executeTime;
    }
};

vector<Task> taskQueue;

long long getCurrentTime()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ==========================================
// 6. MAIN & SETUP
// ==========================================

int main()
{
    auto globalEnv = make_shared<Environment>();

    // --- Define Native Functions ---

    // print("hello")
    auto printFn = make_shared<Value>();
    printFn->type = V_NATIVE;
    printFn->nativeFn = [](vector<shared_ptr<Value>> args)
    {
        for (auto a : args)
            cout << a->toString() << " ";
        cout << endl;
        return make_shared<Value>(); // returns null
    };
    globalEnv->define("print", printFn);

    // setTimeout(callback, ms)
    auto timeoutFn = make_shared<Value>();
    timeoutFn->type = V_NATIVE;
    timeoutFn->nativeFn = [&](vector<shared_ptr<Value>> args)
    {
        if (args.size() < 2 || args[0]->type != V_FUNC)
            return make_shared<Value>();

        long long delay = (long long)args[1]->numVal;
        auto callback = args[0];

        // Push to Task Queue
        Task t;
        t.executeTime = getCurrentTime() + delay;
        t.callback = [callback, globalEnv]()
        {
            // Execute the closure body
            auto scope = make_shared<Environment>(callback->closure);
            callback->body->eval(scope);
        };
        taskQueue.push_back(t);

        return make_shared<Value>();
    };
    globalEnv->define("setTimeout", timeoutFn);

    cout << "--- JS Engine V8-Mini (Async supported) ---" << endl;
    cout << "Enter code. Type 'run' to execute." << endl;

    string code, line;
    while (true)
    {
        getline(cin, line);
        if (line == "exit")
            break;
        if (line == "run")
        {
            try
            {
                Parser parser(code);
                auto stmts = parser.parse();

                // 1. Run Synchronous Code
                for (auto &stmt : stmts)
                {
                    if (stmt)
                        stmt->eval(globalEnv);
                }

                // 2. Run Event Loop (Async)
                if (!taskQueue.empty())
                {
                    cout << "[Event Loop] Processing async tasks..." << endl;
                    while (!taskQueue.empty())
                    {
                        long long now = getCurrentTime();
                        bool ranTask = false;

                        // Simple loop to find ready tasks (inefficient but works for demo)
                        for (auto it = taskQueue.begin(); it != taskQueue.end();)
                        {
                            if (now >= it->executeTime)
                            {
                                it->callback();
                                it = taskQueue.erase(it);
                                ranTask = true;
                            }
                            else
                            {
                                ++it;
                            }
                        }

                        if (!ranTask && !taskQueue.empty())
                        {
                            this_thread::sleep_for(chrono::milliseconds(10));
                        }
                    }
                }
            }
            catch (exception &e)
            {
                cout << "Runtime Error: " << e.what() << endl;
            }
            code = "";
            cout << "\nReady." << endl;
        }
        else
        {
            code += line + "\n";
        }
    }
    return 0;
}