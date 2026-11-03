#include <gtest/gtest.h>

#include "lms/error.hpp"
#include "lms/password.hpp"
#include "lms/user.hpp"

using namespace lms;

class PasswordTest : public ::testing::Test {
 protected:
  void SetUp() override { setHashIterations(1000); }
};

TEST_F(PasswordTest, CorrectPasswordVerifies) {
  std::string h = hashPassword("Secret#123");
  EXPECT_TRUE(verifyPassword("Secret#123", h));
  EXPECT_FALSE(verifyPassword("secret#123", h));
}

TEST_F(PasswordTest, HashIsSaltedAndNeverContainsThePassword) {
  std::string a = hashPassword("Secret#123");
  std::string b = hashPassword("Secret#123");
  EXPECT_NE(a, b);
  EXPECT_EQ(a.find("Secret#123"), std::string::npos);
  EXPECT_EQ(a.rfind("pbkdf2$", 0), 0u);
}

TEST_F(PasswordTest, MalformedHashesAreRejectedNotThrown) {
  EXPECT_FALSE(verifyPassword("x", ""));
  EXPECT_FALSE(verifyPassword("x", "plaintext"));
  EXPECT_FALSE(verifyPassword("x", "pbkdf2$abc$00$00"));
  EXPECT_FALSE(verifyPassword("x", "pbkdf2$10$zz$zz"));
  EXPECT_FALSE(verifyPassword("x", "pbkdf2$0$00$00"));
}

class UserFactoryTest : public PasswordTest {};

TEST_F(UserFactoryTest, BuildsTheRightSubclassForEachRole) {
  EXPECT_EQ(UserFactory::create(Role::kAdmin, "a", "A", "Passw0rd!")->role(),
            Role::kAdmin);
  EXPECT_EQ(UserFactory::create(Role::kLecturer, "l", "L", "Passw0rd!")->role(),
            Role::kLecturer);
  EXPECT_EQ(UserFactory::create(Role::kHod, "h", "H", "Passw0rd!")->role(),
            Role::kHod);
  EXPECT_EQ(UserFactory::create(Role::kStudent, "s", "S", "Passw0rd!")->role(),
            Role::kStudent);
}

TEST_F(UserFactoryTest, HashesThePasswordAndAuthenticates) {
  auto u = UserFactory::create(Role::kStudent, "pat", "Pat", "Passw0rd!");
  EXPECT_NE(u->record().passwordHash, "Passw0rd!");
  EXPECT_TRUE(u->authenticate("Passw0rd!"));
  EXPECT_FALSE(u->authenticate("wrong"));
}

TEST_F(UserFactoryTest, RejectsBadInput) {
  EXPECT_THROW(UserFactory::create(Role::kStudent, "", "N", "Passw0rd!"),
               ValidationError);
  EXPECT_THROW(UserFactory::create(Role::kStudent, "a b", "N", "Passw0rd!"),
               ValidationError);
  EXPECT_THROW(UserFactory::create(Role::kStudent, "x'; DROP", "N", "Passw0rd!"),
               ValidationError);
  EXPECT_THROW(UserFactory::create(Role::kStudent, "ok", "", "Passw0rd!"),
               ValidationError);
  EXPECT_THROW(UserFactory::create(Role::kStudent, "ok", "N", "short"),
               ValidationError);
}

TEST_F(UserFactoryTest, LockStateFollowsFailedLogins) {
  UserRecord r;
  r.username = "u";
  r.fullName = "U";
  r.role = Role::kStudent;
  r.passwordHash = hashPassword("Passw0rd!");
  r.failedLogins = kMaxFailedLogins - 1;
  EXPECT_FALSE(UserFactory::fromRecord(r)->isLocked());
  r.failedLogins = kMaxFailedLogins;
  EXPECT_TRUE(UserFactory::fromRecord(r)->isLocked());
}

TEST_F(UserFactoryTest, HodCarriesItsDepartment) {
  UserRecord r;
  r.username = "h";
  r.fullName = "H";
  r.role = Role::kHod;
  r.passwordHash = hashPassword("Passw0rd!");
  auto u = UserFactory::fromRecord(r, 7);
  EXPECT_EQ(static_cast<HeadOfDepartment&>(*u).deptId(), 7);
}
