#ifndef JWT_HANDLER_HPP
#define JWT_HANDLER_HPP

#include <string>

class JWTUtils {
 public:
  // Génère un token JWT pour un device_id donné
  static std::string CreateToken(const std::string& device_id);

  // Valide un token JWT et extrait le device_id s'il est valide
  static bool ValidateToken(const std::string& token, std::string& device_id);

 private:
  // Clé secrète utilisée pour signer et vérifier les tokens
  static const std::string SECRET_KEY;
};

#endif // JWT_HANDLER_HPP