#ifndef TESTO_HPP
#define TESTO_HPP

#include <string>
#include <iostream>

namespace Testo {
    class Testo
    {
    public:
        int yo;

        void hei();
    protected:
        std::string hello;
    };

    void Testo::hei()
    {
        std::cout << "Hello World!" << std::endl;
    }
}
#endif
