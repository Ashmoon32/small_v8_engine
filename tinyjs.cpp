#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <cmath>

using namespace std;

// --- 1. DATA STRUCTURES (The "Memory") ---

enum ValueType
{
    T_NUM,
    T_STR,
    T_BOOL,
    T_NULL
};

struct Value
{
    ValueType type = T_NULL;
    double numVal = 0;
    string strVal;
    bool boolVal = false;

    // Helper to print value
    string toString() const
    {
        if (type == T_NUM)
        {
            // Remove trailing zeros for display
            string s = to_string(numVal);
            return s.substr(0, s.find_last_not_of('0') + 1);
        }
        if (type == T_STR)
            return strVal;
        if (type == T_BOOL)
            return boolVal ? "true" : "false";
        return "null";
    }

    // Truthy check for 'if' statements
    bool isTruthy() const
    {
        if (type == T_BOOL)
            return boolVal;
        if (type == T_NUM)
            return numVal != 0;
        if (type == T_STR)
            return !strVal.empty();
        return false;
    }
};

struct Variable
{
    Value val;
    bool isConst;
};

// --- 2. THE LEXER (The "Scanner") ---
// Breaks string "let x = 10" into ["let", "x", "=", "10"]

enum TokenType
{
    ID,
    NUMBER,
    STRING,
    LET,
    CONST,
    IF,
    ELSE,
    WHILE,
    PRINT,
    PLUS,
    MINUS,
    MUL,
    DIV,
    ASSIGN,
    EQ,
    GT,
    LT,
    AND,
    OR,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    SEMI,
    END
};

struct Token
{
    TokenType type;
    string text;
};

class Lexer
{
    string src;
    size_t pos = 0;

public:
    Lexer(string s) : src(s) {}

    Token nextToken()
    {
        while (pos < src.size() && isspace(src[pos]))
            pos++;
        if (pos >= src.size())
            return {END, ""};

        char current = src[pos];

        // Numbers
        if (isdigit(current))
        {
            string numStr;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.'))
            {
                numStr += src[pos++];
            }
            return {NUMBER, numStr};
        }

        // Strings
        if (current == '"')
        {
            pos++; // skip opening quote
            string str;
            while (pos < src.size() && src[pos] != '"')
            {
                str += src[pos++];
            }
            pos++; // skip closing quote
            return {STRING, str};
        }

        // Identifiers & Keywords
        if (isalpha(current))
        {
            string id;
            while (pos < src.size() && isalnum(src[pos]))
            {
                id += src[pos++];
            }
            if (id == "let")
                return {LET, id};
            if (id == "const")
                return {CONST, id};
            if (id == "if")
                return {IF, id};
            if (id == "else")
                return {ELSE, id};
            if (id == "while")
                return {WHILE, id};
            if (id == "print")
                return {PRINT, id};
            return {ID, id};
        }

        pos++; // Consume symbol
        switch (current)
        {
        case '+':
            return {PLUS, "+"};
        case '-':
            return {MINUS, "-"};
        case '*':
            return {MUL, "*"};
        case '/':
            return {DIV, "/"};
        case '(':
            return {LPAREN, "("};
        case ')':
            return {RPAREN, ")"};
        case '{':
            return {LBRACE, "{"};
        case '}':
            return {RBRACE, "}"};
        case ';':
            return {SEMI, ";"};
        case '=':
            if (pos < src.size() && src[pos] == '=')
            {
                pos++;
                return {EQ, "=="};
            }
            return {ASSIGN, "="};
        case '&':
            if (pos < src.size() && src[pos] == '&')
            {
                pos++;
                return {AND, "&&"};
            }
            break;
        case '|':
            if (pos < src.size() && src[pos] == '|')
            {
                pos++;
                return {OR, "||"};
            }
            break;
        case '>':
            return {GT, ">"};
        case '<':
            return {LT, "<"};
        }

        cout << "Unknown Token: " << current << endl;
        return {END, ""};
    }
};

// --- 3. THE INTERPRETER (The "Brain") ---

class Interpreter
{
    Lexer lexer;
    Token currentToken;
    // Stack of Scopes (Global -> Block -> Block)
    vector<map<string, Variable>> scopes;

public:
    Interpreter(string src) : lexer(src)
    {
        scopes.push_back({}); // Global Scope
        currentToken = lexer.nextToken();
    }

    void eat(TokenType t)
    {
        if (currentToken.type == t)
        {
            currentToken = lexer.nextToken();
        }
        else
        {
            throw runtime_error("Unexpected token: " + currentToken.text);
        }
    }

    // Variable Management
    Variable *findVar(string name)
    {
        // Look in scopes from inner to outer
        for (int i = scopes.size() - 1; i >= 0; i--)
        {
            if (scopes[i].count(name))
            {
                return &scopes[i][name];
            }
        }
        return nullptr;
    }

    void declareVar(string name, Value val, bool isConst)
    {
        if (scopes.back().count(name))
            throw runtime_error("Variable '" + name + "' already declared.");
        scopes.back()[name] = {val, isConst};
    }

    // --- Expression Parsing (PEMDAS logic) ---

    Value factor()
    {
        Token t = currentToken;
        if (t.type == NUMBER)
        {
            eat(NUMBER);
            return {T_NUM, stod(t.text)};
        }
        if (t.type == STRING)
        {
            eat(STRING);
            return {T_STR, 0, t.text};
        }
        if (t.type == ID)
        {
            eat(ID);
            Variable *v = findVar(t.text);
            if (!v)
                throw runtime_error("Undefined variable: " + t.text);
            return v->val;
        }
        if (t.type == LPAREN)
        {
            eat(LPAREN);
            Value v = expression();
            eat(RPAREN);
            return v;
        }
        throw runtime_error("Unexpected factor: " + t.text);
    }

    Value term()
    {
        Value left = factor();
        while (currentToken.type == MUL || currentToken.type == DIV)
        {
            TokenType op = currentToken.type;
            eat(op);
            Value right = factor();
            if (op == MUL)
                left.numVal *= right.numVal;
            else
                left.numVal /= right.numVal;
        }
        return left;
    }

    Value additive()
    {
        Value left = term();
        while (currentToken.type == PLUS || currentToken.type == MINUS)
        {
            TokenType op = currentToken.type;
            eat(op);
            Value right = term();
            if (left.type == T_STR || right.type == T_STR)
            {
                left = {T_STR, 0, left.toString() + right.toString()};
            }
            else if (op == PLUS)
            {
                left.numVal += right.numVal;
            }
            else
            {
                left.numVal -= right.numVal;
            }
        }
        return left;
    }

    Value comparison()
    {
        Value left = additive();
        while (currentToken.type == GT || currentToken.type == LT || currentToken.type == EQ)
        {
            TokenType op = currentToken.type;
            eat(op);
            Value right = additive();
            bool res = false;
            if (op == GT)
                res = left.numVal > right.numVal;
            if (op == LT)
                res = left.numVal < right.numVal;
            if (op == EQ)
                res = left.numVal == right.numVal; // Simplified equality
            left = {T_BOOL, 0, "", res};
        }
        return left;
    }

    Value expression()
    {
        return comparison();
    }

    // --- Statement Parsing ---

    void block()
    {
        eat(LBRACE);
        scopes.push_back({}); // New Scope
        while (currentToken.type != RBRACE && currentToken.type != END)
        {
            statement();
        }
        scopes.pop_back(); // End Scope
        eat(RBRACE);
    }

    void statement()
    {
        if (currentToken.type == LET || currentToken.type == CONST)
        {
            bool isConst = (currentToken.type == CONST);
            eat(isConst ? CONST : LET);
            string name = currentToken.text;
            eat(ID);
            eat(ASSIGN);
            Value v = expression();
            declareVar(name, v, isConst);
            eat(SEMI);
        }
        else if (currentToken.type == PRINT)
        {
            eat(PRINT);
            Value v = expression();
            cout << v.toString() << endl;
            eat(SEMI);
        }
        else if (currentToken.type == IF)
        {
            eat(IF);
            eat(LPAREN);
            Value cond = expression();
            eat(RPAREN);
            if (cond.isTruthy())
            {
                if (currentToken.type == LBRACE)
                    block();
                else
                    statement();
            }
            else
            {
                // Skip the true block
                // (Note: In a full compiler we would parse but not execute.
                // In this simple interpreter, we skip tokens.
                // A true skip implementation is complex, so we cheat:
                // We actually execute both paths in this simple version unless we use blocks.
                // NOTE: For a perfect IF/ELSE skip in a 1-file Interpreter,
                // we need an AST. Since we are doing direct-execution,
                // IFs are tricky. We will assume the user uses Blocks {}.)
                int braces = 0;
                while (true)
                {
                    if (currentToken.type == LBRACE)
                        braces++;
                    if (currentToken.type == RBRACE)
                    {
                        braces--;
                        if (braces == 0)
                        {
                            eat(RBRACE);
                            break;
                        }
                    }
                    if (currentToken.type == END)
                        break;
                    currentToken = lexer.nextToken();
                }
            }
            if (currentToken.type == ELSE)
            {
                eat(ELSE);
                if (!cond.isTruthy())
                {
                    if (currentToken.type == LBRACE)
                        block();
                    else
                        statement();
                }
            }
        }
        else if (currentToken.type == WHILE)
        {
            // Saving position to loop back requires reading the file into a list of tokens first
            // or seeking the file pointer. For this demo, WHILE is hard in single-pass.
            // But we can support a "Do once" or simple logic.
            // **Correction**: To make WHILE work here, we would need to store the Token list.
            cout << "Warning: 'while' loops require AST architecture (skipped in this version)." << endl;
            // Consume tokens to avoid crash
            eat(WHILE);
            while (currentToken.type != SEMI && currentToken.type != RBRACE)
                currentToken = lexer.nextToken();
        }
        else if (currentToken.type == LBRACE)
        {
            block();
        }
        else if (currentToken.type == ID)
        {
            // Assignment: x = 10;
            string name = currentToken.text;
            eat(ID);
            eat(ASSIGN);
            Value v = expression();
            Variable *var = findVar(name);
            if (!var)
                throw runtime_error("Variable not declared: " + name);
            if (var->isConst)
                throw runtime_error("Cannot reassign const variable: " + name);
            var->val = v;
            eat(SEMI);
        }
        else
        {
            eat(SEMI); // Empty statement
        }
    }

    void run()
    {
        while (currentToken.type != END)
        {
            try
            {
                statement();
            }
            catch (const exception &e)
            {
                cout << "Error: " << e.what() << endl;
                break;
            }
        }
    }
};

int main()
{
    cout << "--- TinyJS Interpreter (Type 'exit' to quit) ---" << endl;
    cout << "Supports: let, const, print, if/else, math, strings" << endl;
    cout << "Enter your code (one line or multiple, end with 'run'):" << endl;

    string line, fullCode;
    while (true)
    {
        getline(cin, line);
        if (line == "exit")
            break;
        if (line == "run")
        {
            Interpreter interpreter(fullCode);
            interpreter.run();
            fullCode = "";
            cout << "\nReady for next code block:" << endl;
        }
        else
        {
            fullCode += line + " ";
        }
    }
    return 0;
}