#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <unistd.h>


using namespace std;

/// A lexed word. isOperator is true only for unquoted shell operators
/// (|, <, >, >>, 2>, ...), so a quoted ">>" stays a plain word.
struct Token {
    std::string text;
    bool isOperator = false;
};

std::vector<Token> tokenize(const std::string &content);
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
    if (op != ">" && op != ">>" && op != "<" && op != ">&" && op != "<&" && op != "<<" && op != "<<-") {
        return false;
    }
    if (digits > 0) {
        fd = std::stoi(token.substr(0, digits));
    } else {
        fd = (op[0] == '<') ? 0 : 1;
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

        std::vector<Token> tokens = tokenize(line);
        std::vector<std::string> argv;
        std::vector<Redirect> redirs;

        if (tokens.size() == 1 && !tokens[0].isOperator && tokens[0].text.rfind("ERR", 0) == 0) {
            std::cout << tokens[0].text << "\n";
            continue;
        }

        std::string syntaxError;
        for (size_t i = 0; i < tokens.size(); i++) {
            Redirect r;
            if (tokens[i].isOperator && parseRedirectOp(tokens[i].text, r.fd, r.op)) {
                // a redirect needs a plain word as target
                if (i + 1 >= tokens.size()) {
                    syntaxError = "newline";
                    break;
                }
                if (tokens[i + 1].isOperator) {
                    syntaxError = tokens[i + 1].text;
                    break;
                }
                r.target = tokens[i + 1].text;
                redirs.push_back(r);
                i++;
            } else if (tokens[i].isOperator) {
                // pipes, ';' etc. are not supported yet
                syntaxError = tokens[i].text;
                break;
            } else {
                argv.push_back(tokens[i].text);
            }
        }

        if (!syntaxError.empty()) {
            std::cout << "ERR syntax error near unexpected token '" << syntaxError << "'\n";
            continue;
        }

        std::cout << "argv: ";
        for (size_t i = 0; i < argv.size(); i++) {
            if (i > 0) {
                std::cout << " ";
            }
            std::cout << encloseToken(argv[i]);
        }
        std::cout << "\n";

        for (const Redirect &r : redirs) {
            std::cout << "redir fd=" << r.fd << " op=" << r.op
                      << " target=" << r.target << "\n";
        }
    }
}

/// Splits a command line into tokens, handling whitespace, backslash escapes,
/// single/double quotes and pipes. Returns the list of tokens, or a single
/// "ERR ..." entry on a syntax error.
std::vector<Token> tokenize(const std::string &content) {
    std::vector<Token> tokens;
    std::string current{};
    bool inToken = false;
    size_t cursor = 0;
    size_t commandStart = 0;

    while (cursor < content.length()) {
        char c = content[cursor];

        if (c == ' ' || c == '\t') {
            if (inToken) {
                tokens.push_back(Token{current});
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
                return {Token{"ERR unterminated quote"}};
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
                return {Token{"ERR unterminated quote"}};
            }
        }
        else if (c == '|') {
            if (inToken) {
                tokens.push_back(Token{current});
                current.clear();
                inToken = false;
            }
            if (tokens.size() == commandStart) {
                return {Token{"ERR syntax error: empty command in pipeline"}};
            }
            tokens.push_back(Token{"|", true});
            commandStart = tokens.size();

            size_t next = content.find_first_not_of(" \t\n\r\v\f", cursor + 1);

            if (next == std::string::npos) {
                if (!isatty(STDIN_FILENO)) {
                    return {Token{"ERR syntax error: empty command in pipeline"}};
                }
                std::string continuation{};
                std::cerr << "Please provide additional Commands: " << std::flush;
                if (!std::getline(std::cin, continuation)) {
                    return {Token{"ERR syntax error: empty command in pipeline"}};
                }
                std::vector<Token> rest = tokenize(continuation);
                if (rest.empty()) {
                    return {Token{"ERR syntax error: empty command in pipeline"}};
                }
                if (rest[0].text.rfind("ERR", 0) == 0) {
                    return rest;
                }
                tokens.insert(tokens.end(), rest.begin(), rest.end());
                commandStart = tokens.size();
            }
            cursor++;
        }
        else if (c == ';') {
            if (inToken) {
                tokens.push_back(Token{current});
                current.clear();
                inToken = false;
            }
            tokens.push_back(Token{";", true});
            cursor++;
        }
        else if (c == '>' || c == '<') {
            // an all-digit word directly before the operator is the fd ("3<&0")
            std::string prefix;
            if (inToken && !current.empty() &&
                std::all_of(current.begin(), current.end(), [](char ch) { return isdigit(static_cast<unsigned char>(ch)); })) {
                prefix = current;
                current.clear();
                inToken = false;
            } else if (inToken) {
                tokens.push_back(Token{current});
                current.clear();
                inToken = false;
            }
            static const char *ops[] = {"<<-", ">>", ">&", "<<", "<&", ">", "<"};
            std::string op;
            for (const char *candidate : ops) {
                if (content.compare(cursor, std::string(candidate).size(), candidate) == 0) {
                    op = candidate;
                    break;
                }
            }
            tokens.push_back(Token{prefix + op, true});
            cursor += op.size();
        }
        else {
            inToken = true;
            current += c;
            cursor++;
        }
    }

    if (inToken) {
        tokens.push_back(Token{current});
    }

    return tokens;
}



/// Wraps a token in square brackets, e.g. "ls" -> "[ls]".
std::string encloseToken(const std::string &token) {
    return "[" + token + "]";
}
