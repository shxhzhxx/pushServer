#include <iostream>
#include <unordered_map>
 
int main()
{
    // Create an unordered_map of three strings (that map to strings)
    std::unordered_map<int, int> u = {
        {1,111},
        {2,222},
        {3,333}
    };
    
    
    // Add two new entries to the unordered_map
    u.insert({4,444});
    u[5] = 555;
    u[5] = 999;
    u.erase(2);
 
    // Iterate and print keys and values of unordered_map
    for( const auto& n : u ) {
        std::cout << "Key:[" << n.first << "] Value:[" << n.second << "]\n";
    }
 
 
    // Output values by key
    std::cout << "The HEX of color RED is:[" << u[4] << "]\n";
    std::cout << "The HEX of color BLACK is:[" << u[5] << "]\n";
    std::cout << "TEST:[" << (u.find(5)==u.end()?"true":"false") << "]\n";
 
    return 0;
}