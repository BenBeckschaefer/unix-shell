//#include <bits/stdc++.h>
#include <string>
#include <vector>
#include <iostream>



std::string tokenize(std::string &content);
std::string encloseToken(std::string &token);
std::string getNextStandardToken(const std::string& content, size_t& start);




int main() {

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
    }
    
        std::string test = "echo hello world";
        std::string test2 = "ls -la /tmp";
        std::string test3 = "'Dies ist ein Tokentest'";

        std::cout << tokenize(test) << std::endl;
        std::cout << tokenize(test2) << std::endl;
        std::cout << tokenize(test3) << std::endl;

}

std::string tokenize(std::string &content ) {
   size_t cursor = 0; // Merkt sich, wo wir im String stehen
   std::string out{};

// Beispiel-Schleife oder schrittweise Abfrage:
while (cursor < content.length()) {

    if (content[cursor] == ' ') {
        cursor++; 
        continue;
    } 
 else if (content[cursor] == '"') {
    size_t endQuote = content.find('"', cursor + 1);

    if (endQuote != std::string::npos) {
        std::string token = content.substr(cursor + 1, endQuote - (cursor + 1));
        out += encloseToken(token);
        cursor = endQuote + 1; // Cursor hinter das schließende " setzen
    } else {
        // Fehlerfall: Kein schließendes " gefunden
        std::string token = content.substr(cursor + 1);
        out += encloseToken(token);
        cursor = content.length();
    }
}
    else if (content[cursor] == '\'') {

    size_t endQuote = content.find('\'', cursor + 1);

    if (endQuote != std::string::npos) {
        std::string token = content.substr(cursor + 1, endQuote - (cursor + 1));
        
        out += encloseToken(token);

        cursor = endQuote + 1;
    } else {
        std::string token = content.substr(cursor + 1);
        out += encloseToken(token);
        cursor = content.length(); // Beendet die äußere Schleife
    }
    } 
    else {
        std::string token = getNextStandardToken(content, cursor);
        out += encloseToken(token);
    }
}
    return out;
    
}



std::string encloseToken(std::string &token) {
    return "[" + token + "]";
}


std::string getNextStandardToken(const std::string& content, size_t& start) {
    while (start < content.length() && content[start] == ' ') {
        start++;
    }

    // Falls wir am Ende des Strings angekommen sind
    if (start >= content.length()) {
        return "";
    }

    // 2. Finde das Ende des Tokens (nächstes Leerzeichen oder String-Ende)
    size_t i = start;
    while (i < content.length() && content[i] != ' ') {
        i++;
    }

    // 3. Schneide genau dieses eine Token aus
    std::string token = content.substr(start, i - start);

    // 4. Setze den Start-Cursor weiter für den NÄCHSTEN Aufruf
    start = i;

    return token;
}

