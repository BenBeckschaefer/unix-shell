#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <unistd.h>


using namespace std;

std::vector<std::string> tokenize(const std::string &content);
std::string encloseToken(const std::string &token);

struct Redirect {
    int fd;
    std::string op;
    std::string target;
};

/// Recognises redirect operators such as ">", ">>", "<", "2>" or "1>>".
/// Returns true and fills fd/op if the token is one.
static bool parseRedirectOp(const std::string &token, int &fd, std::string &op) {
    size_t digits = 0;
    while (digits < token.size() && isdigit(static_cast<unsigned char>(token[digits]))) {
        digits++;
    }
    op = token.substr(digits);
    if (op != ">" && op != ">>" && op != "<") {
        return false;
    }
    if (digits > 0) {
        fd = std::stoi(token.substr(0, digits));
    } else {
        fd = (op == "<") ? 0 : 1;
    }
    return true;
}

/// Reads commands line by line from stdin and prints argv and redirects.
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> tokens = tokenize(line);
        std::vector<std::string> argv;
        std::vector<Redirect> redirs;

        for (size_t i = 0; i < tokens.size(); i++) {
            Redirect r;
            if (parseRedirectOp(tokens[i], r.fd, r.op) && i + 1 < tokens.size()) {
                r.target = tokens[i + 1];
                redirs.push_back(r);
                i++;
            } else {
                argv.push_back(tokens[i]);
            }
        }

        std::cout << "argv: ";
        for (size_t i = 0; i < argv.size(); i++) {
            std::cout << "[" << argv[i] << "] "; 
        }
        std::cout << std::endl;

        for (const Redirect &r : redirs) {
            std::cout << "redir fd=" << r.fd << " op=" << r.op
                      << " target=" << r.target << "\n";
        }
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
        }
        else if (c == '>' || c == '<') {
            bool fdPrefix = c == '>' && inToken && current.size() == 1 && isdigit(static_cast<unsigned char>(current[0]));
            if (fdPrefix) {
                bool append = cursor + 1 < content.length() && content[cursor + 1] == '>';
                tokens.push_back(current + (append ? ">>" : ">"));
                current.clear();
                inToken = false;
                cursor += append ? 2 : 1;
                continue;
            }
            if (inToken) {
                tokens.push_back(current);
                current.clear();
                inToken = false;
            }
            if (c == '>' && cursor + 1 < content.length() && content[cursor + 1] == '>') {
                tokens.push_back(">>");
                cursor += 2;
            } else {
                tokens.push_back(std::string(1, c));
                cursor++;
            }
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
