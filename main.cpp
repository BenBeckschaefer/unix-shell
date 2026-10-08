#include <string>
#include <vector>
#include <iostream>
#include <algorithm>


using namespace std;

std::string tokenize(const std::string &content);
std::string encloseToken(const std::string &token);




int main() {

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        std::string current = tokenize(line);
        std::cout << current << "\n";    
        

       size_t firstPipe = current.find('|');

       


    }



}

std::string tokenize(const std::string &content) {
    std::vector<std::string> tokens;
    std::string current{};
    bool inToken = false; // true, sobald ein Token begonnen hat (auch bei "" oder '')
    size_t cursor = 0;    // Merkt sich, wo wir im String stehen

    while (cursor < content.length()) {
        char c = content[cursor];

        if (c == ' ' || c == '\t') {
            // Whitespace beendet das aktuelle Token
            if (inToken) {
                tokens.push_back(current);
                current.clear();
                inToken = false;
            }
            cursor++;
        }
        else if (c == '\\') {
            // Backslash außerhalb von Quotes: nächstes Zeichen wörtlich übernehmen
            inToken = true;
            if (cursor + 1 < content.length()) {
                current += content[cursor + 1];
            }
            cursor += 2;
        }
        else if (c == '\'') {
            // Single Quotes: alles bis zum schließenden ' wörtlich
            size_t endQuote = content.find('\'', cursor + 1);
            if (endQuote == std::string::npos) {
                return "ERR unterminated quote";
            }
            inToken = true;
            current += content.substr(cursor + 1, endQuote - (cursor + 1));
            cursor = endQuote + 1;
        }
        else if (c == '"') {
            // Double Quotes: \" \\ \$ \` werden escaped, sonst wörtlich
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
                return "ERR unterminated quote";
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

    std::string out{};
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0) {
            out += " ";
        }
        out += encloseToken(tokens[i]);
    }
    return out;
}



std::string encloseToken(const std::string &token) {
    return "[" + token + "]";
}
