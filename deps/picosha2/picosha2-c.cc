#include "picosha2-c.h"
#include "picosha2.h"

#include <array>
#include <cstring>
#include <string>

extern "C" {

void picosha2_256(const char *buffer, int len, char *dest) {
  std::array<uint8_t, picosha2::k_digest_size> hash;
  picosha2::hash256(buffer, buffer + len, std::begin(hash), std::end(hash));
  std::string sha256 =
      picosha2::bytes_to_hex_string(std::begin(hash), std::end(hash));
  std::memcpy(dest, sha256.c_str(), sha256.size() + 1);
}
}
