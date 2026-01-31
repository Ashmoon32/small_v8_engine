#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

using namespace std;

// This is our "Memory" - it stores variables
map<string, int> variables;

void execute(string line)
{
    stringstream ss(line);
    string command;
    ss >> command;

    if (command == "let")
    {
        // Handle: let x = 10
        string varName, eq;
        int value;
        ss >> varName >> eq >> value;
        variables[varName] = value;
    }
    else if (command == "print")
    {
        // Handle: print x
        string varName;
        ss >> varName;
        if (variables.count(varName))
        {
            cout << ">> " << variables[varName] << endl;
        }
        else
        {
            cout << ">> Error: Variable '" << varName << "' not found." << endl;
        }
    }
    else if (command == "add")
    {
        // Handle: add x 5 (Adds 5 to x)
        string varName;
        int value;
        ss >> varName >> value;
        if (variables.count(varName))
        {
            variables[varName] += value;
        }
    }
}

int main()
{
    string line;
    cout << "TinyLang Interpreter v1.0" << endl;
    cout << "Commands: 'let x = 10', 'print x', 'add x 5', 'exit'" << endl;

    while (true)
    {
        cout << "> ";
        getline(cin, line);
        if (line == "exit")
            break;
        if (line.empty())
            continue;
        execute(line);
    }
    return 0;
}