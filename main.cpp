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

struct Heredoc {
    std::string delimiter;
    bool expand = true;
    bool stripTabs = false;
    std::vector<std::string> body;
};

/// Finds every unquoted "<<" / "<<-" on a command line and records the
/// delimiter word (quotes removed). A quoted delimiter turns expand off.
static std::vector<Heredoc> findHeredocs(const std::string &line, std::string &syntaxError) {
    std::vector<Heredoc> found;
    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];
        if (c == '\\') {
            i += 2;
        } else if (c == '\'') {
            size_t end = line.find('\'', i + 1);
            i = (end == std::string::npos) ? line.size() : end + 1;
        } else if (c == '"') {
            i++;
            while (i < line.size() && line[i] != '"') {
                i += (line[i] == '\\') ? 2 : 1;
            }
            i++;
        } else if (c == '<' && line.compare(i, 2, "<<") == 0) {
            Heredoc h;
            i += 2;
            if (i < line.size() && line[i] == '-') {
                h.stripTabs = true;
                i++;
            }
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                i++;
            }
            if (i >= line.size()) {
                syntaxError = "newline";
                return found;
            }
            if (std::string("|;<>&()").find(line[i]) != std::string::npos) {
                syntaxError = line.substr(i, (line[i] == '>' || line[i] == '<') && line.compare(i, 2, std::string(2, line[i])) == 0 ? 2 : 1);
                return found;
            }
            // read the delimiter word, dropping quotes and remembering them
            while (i < line.size() && !isspace(static_cast<unsigned char>(line[i])) &&
                   line[i] != '|' && line[i] != ';' && line[i] != '<' && line[i] != '>') {
                char d = line[i];
                if (d == '\\') {
                    h.expand = false;
                    if (i + 1 < line.size()) {
                        h.delimiter += line[i + 1];
                    }
                    i += 2;
                } else if (d == '\'' || d == '"') {
                    h.expand = false;
                    size_t end = line.find(d, i + 1);
                    if (end == std::string::npos) {
                        end = line.size();
                    }
                    h.delimiter += line.substr(i + 1, end - i - 1);
                    i = end + 1;
                } else {
                    h.delimiter += d;
                    i++;
                }
            }
            found.push_back(h);
        } else {
            i++;
        }
    }
    return found;
}

/// Reads script lines from stdin. For each command line prints the line and,
/// for every here-document on it, the delimiter, flags and body.
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        std::cout << "CMD " << line << "\n";

        std::vector<Token> tokens = tokenize(line);
        if (tokens.size() == 1 && !tokens[0].isOperator && tokens[0].text == "ERR unterminated quote") {
            std::cout << tokens[0].text << "\n";
            continue;
        }

        std::string syntaxError;
        std::vector<Heredoc> heredocs = findHeredocs(line, syntaxError);
        if (!syntaxError.empty()) {
            std::cout << "ERR syntax error near unexpected token '" << syntaxError << "'\n";
            continue;
        }
        for (Heredoc &h : heredocs) {
            bool terminated = false;
            std::string bodyLine;
            while (std::getline(std::cin, bodyLine)) {
                if (h.stripTabs) {
                    size_t firstNonTab = bodyLine.find_first_not_of('\t');
                    bodyLine = (firstNonTab == std::string::npos) ? "" : bodyLine.substr(firstNonTab);
                }
                if (bodyLine == h.delimiter) {
                    terminated = true;
                    break;
                }
                h.body.push_back(bodyLine);
            }

            std::cout << "HEREDOC " << h.delimiter
                      << " expand=" << (h.expand ? "yes" : "no")
                      << " strip=" << (h.stripTabs ? "yes" : "no") << "\n";
            for (const std::string &b : h.body) {
                std::cout << "|" << b << "\n";
            }
            if (!terminated) {
                std::cout << "UNTERMINATED " << h.delimiter << "\n";
                return 0;
            }
            std::cout << "END\n";
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
