#include <iostream>

#include "dazzle.h"

using dazzle::Updater;

int main() {
  auto& updater = Updater::GetInstance();

  std::string configured = updater.GetFeedUrl();
  std::cout << "Configured feed URL: " << (configured.empty() ? "(none)" : configured)
            << std::endl;

  updater.SetFeedUrl("https://example.com/appcast.xml");
  std::cout << "Feed URL:            " << updater.GetFeedUrl() << std::endl;

  return 0;
}
