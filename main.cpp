#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <unistd.h>


using namespace std;

std::vector<std::string> tokenize(const std::string &content);
std::string encloseToken(const std::string &token);




/// Reads commands line by line from stdin and prints the tokenized result.
int main() {

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
     std::cout << "argv=[";
       std::vector<std::string> current = tokenize(line);
       for (size_t i = 0; i < current.size(); i++) {
           if (i > 0) {
               std::cout << " ";
           }
           std::cout << '\''<< current[i] << '\''; 

           if(i  < current.size() - 1) {
                std::cout << ",";
           }
       }
       std::cout << "]" <<"\n";




   }



}

/// Splits a command line into tokens, handling whitespace, backslash escapes,
/// single/double quotes and pipes. Returns the list of tokens, or a single
/// "ERR ..." entry on a syntax error.
std::vector<std::string> tokenize(const std::string &content) {
    std::vector<std::string> tokens;
    std::string current{};
    bool inToken = false;
    size_t cursor = 0;
    size_t commandStart = 0;

    while (cursor < content.length()) {
        char c = content[cursor];

        if (c == ' ' || c == '\t') {
            if (inToken) {
                tokens.push_back(current);
                current.clear();
                inToken = false;
            }
            cursor++;
        }
        else if (c == '\\') {
            inToken = true;
            if (cursor + 1 < content.length()) {
                current += content[cursor + 1];
            }
            cursor += 2;
        }
        else if (c == '\'') {
            size_t endQuote = content.find('\'', cursor + 1);
            if (endQuote == std::string::npos) {
                return {"ERR unterminated quote"};
            }
            inToken = true;
            current += content.substr(cursor + 1, endQuote - (cursor + 1));
            cursor = endQuote + 1;
        }
        else if (c == '"') {
            inToken = true;
            cursor++;
            bool closed = false;
            while (cursor < content.length()) {
                char q = content[cursor];
                if (q == '"') {
                    closed = true;
                    cursor++;
                    break;
                }
                if (q == '\\' && cursor + 1 < content.length()) {
                    char next = content[cursor + 1];
                    if (next == '"' || next == '\\' || next == '$' || next == '`') {
                        current += next;
                        cursor += 2;
                        continue;
                    }
                }   
                current += q;
                cursor++;
            }
            if (!closed) {
                return {"ERR unterminated quote"};
            }
        }
        else if (c == '|') {
            if (inToken) {
                tokens.push_back(current);
                current.clear();
                inToken = false;
            }
            if (tokens.size() == commandStart) {
                return {"ERR syntax error: empty command in pipeline"};
            }
            tokens.push_back("|");
            commandStart = tokens.size();

            size_t next = content.find_first_not_of(" \t\n\r\v\f", cursor + 1);

            if (next == std::string::npos) {
                if (!isatty(STDIN_FILENO)) {
                    return {"ERR syntax error: empty command in pipeline"};
                }
                std::string continuation{};
                std::cerr << "Please provide additional Commands: " << std::flush;
                if (!std::getline(std::cin, continuation)) {
                    return {"ERR syntax error: empty command in pipeline"};
                }
                std::vector<std::string> rest = tokenize(continuation);
                if (rest.empty()) {
                    return {"ERR syntax error: empty command in pipeline"};
                }
                if (rest[0].rfind("ERR", 0) == 0) {
                    return rest;
                }
                tokens.insert(tokens.end(), rest.begin(), rest.end());
                commandStart = tokens.size();
            }
            cursor++;
        }else if (c == '>' || c == '<') {
            char next = current[cursor + 1];
            if(next == '>') {
                current += next;
                cursor++;
            }
            tokens.push_back(current);
            current.clear();
            inToken = false;

            if (tokens.size() == commandStart) {
                return {"ERR syntax error: empty command in pipeline"};
            }
            commandStart = tokens.size();
        }
        else {
            inToken = true;
            current += c;
            cursor++;
        }
    }

    if (inToken) {
        tokens.push_back(current);
    }

    return tokens;
}



/// Wraps a token in square brackets, e.g. "ls" -> "[ls]".
std::string encloseToken(const std::string &token) {
    return "[" + token + "]";
}
