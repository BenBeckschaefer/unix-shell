//#include <bits/stdc++.h>
#include <string>
#include <vector>
#include <iostream>



std::string tokenize(std::string &content);
std::string encloseToken(std::string &token);



int main() {
    /*
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
    }
    */
        std::string test = "echo hello world";
        std::string test2 = "Input:  ls -la /tmp";

        std::cout << tokenize(test) << std::endl;
        std::cout << tokenize(test2) <<std::endl;

}

std::string tokenize(std::string &content ) {
    size_t start = 0;
    
    std::string token{};
    std::string out{};
    
    
    for(int i {}; i <= content.length(); i++  ) {

        if(content[i] == ' ' ) {
           if (i > start) {
            std::string token = content.substr(start, i - start);
            out += encloseToken(token);
        }
        
        // Der Start für das NÄCHSTE Token liegt direkt nach dem Leerzeichen
        start = i + 1;

        } else if (content[i] == '\"') {
 

        } else if(content[i] == '\'') {


        }
    }

    return out;
    
    
}



std::string encloseToken(std::string &token) {
    return "[" + token + "]";
}


