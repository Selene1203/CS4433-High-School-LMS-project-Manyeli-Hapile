#include "lms/password.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdio>
#include <vector>

#include "lms/error.hpp"

namespace lms {
namespace {

constexpr int kDefaultIterations = 100000;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;

int gIterations = kDefaultIterations;

std::string toHex(const unsigned char* data, std::size_t n) {
  static const char* kDigits = "0123456789abcdef";
  std::string out;
  for (std::size_t i = 0; i < n; ++i) {
    out += kDigits[data[i] >> 4];
    out += kDigits[data[i] & 0x0F];
  }
  return out;
}

bool fromHex(const std::string& s, std::vector<unsigned char>& out) {
  if (s.size() % 2 != 0) return false;
  out.clear();
  for (std::size_t i = 0; i < s.size(); i += 2) {
    unsigned v = 0;
    if (std::sscanf(s.substr(i, 2).c_str(), "%2x", &v) != 1) return false;
    out.push_back(static_cast<unsigned char>(v));
  }
  return true;
}

std::vector<unsigned char> derive(const std::string& password,
                                  const std::vector<unsigned char>& salt,
                                  int iterations) {
  std::vector<unsigned char> key(kHashBytes);
  int ok = PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                             salt.data(), static_cast<int>(salt.size()),
                             iterations, EVP_sha256(),
                             static_cast<int>(key.size()), key.data());
  if (ok != 1) throw Error("password hashing failed");
  return key;
}

}  // namespace

void setHashIterations(int iterations) { gIterations = iterations; }

std::string hashPassword(const std::string& password) {
  std::vector<unsigned char> salt(kSaltBytes);
  if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
    throw Error("could not get random bytes for the salt");
  }
  auto key = derive(password, salt, gIterations);
  return "pbkdf2$" + std::to_string(gIterations) + "$" +
         toHex(salt.data(), salt.size()) + "$" + toHex(key.data(), key.size());
}

bool verifyPassword(const std::string& password, const std::string& stored) {
  // split into four parts on '$'
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (true) {
    std::size_t p = stored.find('$', start);
    if (p == std::string::npos) {
      parts.push_back(stored.substr(start));
      break;
    }
    parts.push_back(stored.substr(start, p - start));
    start = p + 1;
  }
  if (parts.size() != 4 || parts[0] != "pbkdf2") return false;
  int iterations = 0;
  try {
    iterations = std::stoi(parts[1]);
  } catch (...) {
    return false;
  }
  if (iterations < 1) return false;
  std::vector<unsigned char> salt, expected;
  if (!fromHex(parts[2], salt) || !fromHex(parts[3], expected)) return false;
  if (expected.size() != kHashBytes) return false;
  auto actual = derive(password, salt, iterations);
  return CRYPTO_memcmp(actual.data(), expected.data(), kHashBytes) == 0;
}

}  // namespace lms
