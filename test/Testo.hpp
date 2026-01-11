#ifndef TESTO_HPP
#define TESTO_HPP

#include <string>
#include <iostream>
#include <memory>


namespace Andre {
  class Deig {
  public:
    using Delt = std::shared_ptr<Andre::Deig>;
    void kokk();
  };

  class Topping {

  };
}
namespace Testo {
  class Testo {
  public:
    using WeakTopping = std::weak_ptr<Andre::Topping>;

    int yo;

    void hei();
    Andre::Deig toDeig();

    void adapt(std::unique_ptr<Andre::Topping> topping);

    std::shared_ptr<Andre::Deig> share();

    Andre::Deig::Delt shareMore();

    bool accept(Andre::Deig::Delt);

    using UniqueTopping = std::unique_ptr<Andre::Topping>;
    UniqueTopping getTopping();

    WeakTopping getMehTopping();
  protected:
    std::string hello;
  };

  void Testo::hei() {
    std::cout << "Hello World!" << std::endl;
  }
}
#endif
