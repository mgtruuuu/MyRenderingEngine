#include <iostream>
#include <memory>
#include <windows.h>

#include "ExampleApp.h"

int main() {
    Moon::ExampleApp exampleApp;

    if (!exampleApp.Initialize()) {
        std::cerr << "Initialization failed." << std::endl;
        return -1;
    }

    return exampleApp.Run();
}
