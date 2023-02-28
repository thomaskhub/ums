#include "config.h"

/**
 * @brief Validate the input arguments
 *
 * @return true  input arguments check pass
 * @return false check failed
 */
bool Config::validateInput() {
  if (this->primaryInput == NULL) {
    std::cout << "Error: primaryInput argument is null" << std::endl;
    return false;
  }

  if (this->outputUrl == NULL) {
    std::cout << "Error: outputUrl argument is null" << std::endl;
    return false;
  }

  if (this->fillerJpg == NULL) {
    std::cout << "Error : fillerJpg argument is null " << std::endl;
    return false;
  }

  return true;
}