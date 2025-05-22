#ifndef MISC_HELPERS_H
#define MISC_HELPERS_H

#include <iostream>
#include <string>

void PrintHeader(std::string name)
{
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "Running " << name << " App\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n" << std::endl;
}

void PrintFooter(std::string name, int exitCode)
{
    std::cout << "\n----------------------------------------------------------------------------------------------------\n";
    if (exitCode == 0)
    {
        std::cout << name << " App Completed Successfully\n";
    }
    else
    {
        std::cout << name << " App Failed with Exit Code: " << exitCode << "\n";
    }
    std::cout << "----------------------------------------------------------------------------------------------------\n" << std::endl;
}

#endif // MISC_HELPERS_H