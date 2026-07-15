#include <dlfcn.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: NeuramarBundleFactorySmoke <binary> <symbol> <invoke>\n";
        return EXIT_FAILURE;
    }

    void* handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        std::cerr << "could not load bundle binary: " << dlerror() << '\n';
        return EXIT_FAILURE;
    }

    dlerror();
    void* symbol = dlsym(handle, argv[2]);
    if (const auto* error = dlerror())
    {
        std::cerr << "could not find factory symbol: " << error << '\n';
        dlclose(handle);
        return EXIT_FAILURE;
    }

    if (std::string_view(argv[3]) == "invoke")
    {
        const auto factoryFunction = reinterpret_cast<void* (*)()>(symbol);
        if (factoryFunction() == nullptr)
        {
            std::cerr << "factory function returned null\n";
            dlclose(handle);
            return EXIT_FAILURE;
        }
    }

    dlclose(handle);
    std::cout << argv[2] << " loaded successfully\n";
    return EXIT_SUCCESS;
}
