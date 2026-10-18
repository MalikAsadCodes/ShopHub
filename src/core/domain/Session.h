#pragma once

#include "domain/Types.h"

namespace shophub {

// The signed-in user. Services check the session on every call, so role
// rules are enforced in the business layer, not just by hiding buttons.
class Session
{
public:
    Session() = default;
    Session(qint64 userId, QString fullName, QString email, Role role)
        : m_userId(userId), m_fullName(std::move(fullName)), m_email(std::move(email)), m_role(role)
    {}

    bool isValid() const { return m_userId > 0; }
    bool isAdmin() const { return isValid() && m_role == Role::Admin; }
    bool isCustomer() const { return isValid() && m_role == Role::Customer; }

    qint64 userId() const { return m_userId; }
    const QString &fullName() const { return m_fullName; }
    const QString &email() const { return m_email; }
    Role role() const { return m_role; }

private:
    qint64 m_userId = 0;
    QString m_fullName;
    QString m_email;
    Role m_role = Role::Customer;
};

} // namespace shophub
