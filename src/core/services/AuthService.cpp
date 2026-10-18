#include "services/AuthService.h"

#include "common/Validation.h"
#include "services/Access.h"

#include <QtDebug>

namespace shophub {

namespace {
const QString kInvalidCredentials = QStringLiteral("Invalid email or password.");
}

AuthService::AuthService(Repositories &repos, const PasswordHasher &hasher)
    : m_repos(repos), m_hasher(hasher)
{
}

Result<Session> AuthService::login(const QString &email, const QString &password)
{
    const QString normalized = validation::normalizeEmail(email);
    if (normalized.isEmpty() || password.isEmpty())
        return Result<Session>::fail(QStringLiteral("Please enter your email and password."));

    auto found = m_repos.users.findByEmail(normalized);
    if (!found)
        return found.status();

    if (!found.value()) {
        // Spend the same time as a real check so attackers cannot tell
        // which emails are registered by measuring response time.
        static const QString dummy = m_hasher.hash(QStringLiteral("dummy-password-1"));
        m_hasher.verify(password, dummy);
        qInfo() << "Login failed: unknown account";
        return Result<Session>::fail(kInvalidCredentials);
    }

    const UserRecord record = *found.value();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (!record.user.active)
        return Result<Session>::fail(QStringLiteral("This account has been disabled. Please contact support."));

    if (record.lockedUntil.isValid() && record.lockedUntil > now) {
        const qint64 minutes = qMax<qint64>(1, (now.secsTo(record.lockedUntil) + 59) / 60);
        return Result<Session>::fail(
            QStringLiteral("Too many failed attempts. Try again in %1 minute(s).").arg(minutes));
    }

    if (!m_hasher.verify(password, record.passwordHash)) {
        int failed = record.failedLogins + 1;
        QDateTime lockUntil;
        if (failed >= kMaxFailedLogins) {
            lockUntil = now.addSecs(kLockMinutes * 60);
            failed = 0;
            m_repos.audit.log(record.user.id, QStringLiteral("account_locked"), QStringLiteral("user"), record.user.id);
            qInfo() << "Account locked after repeated failed logins, user id" << record.user.id;
        }
        m_repos.users.recordFailedLogin(record.user.id, failed, lockUntil);
        if (lockUntil.isValid())
            return Result<Session>::fail(
                QStringLiteral("Too many failed attempts. Your account is locked for %1 minutes.").arg(kLockMinutes));
        return Result<Session>::fail(kInvalidCredentials);
    }

    m_repos.users.recordSuccessfulLogin(record.user.id);
    if (m_hasher.needsRehash(record.passwordHash))
        m_repos.users.updatePasswordHash(record.user.id, m_hasher.hash(password));
    m_repos.audit.log(record.user.id, QStringLiteral("login"), QStringLiteral("user"), record.user.id);
    qInfo() << "User signed in, id" << record.user.id;

    return Session(record.user.id, record.user.fullName, record.user.email, record.user.role);
}

Result<Session> AuthService::registerCustomer(const QString &fullName, const QString &email, const QString &password)
{
    return createUser(fullName, email, password, Role::Customer, m_hasher, true);
}

Result<Session> AuthService::createFirstAdmin(const QString &fullName, const QString &email, const QString &password)
{
    auto exists = hasAdmin();
    if (!exists)
        return exists.status();
    if (exists.value())
        return Result<Session>::fail(QStringLiteral("An administrator account already exists."));
    return createUser(fullName, email, password, Role::Admin, m_hasher, true);
}

Result<Session> AuthService::createSampleCustomer(const Session &admin, const QString &fullName,
                                                  const QString &email, const QString &password)
{
    if (auto st = access::requireAdmin(admin); !st)
        return st;
    if (password.isEmpty())
        return Result<Session>::fail(QStringLiteral("Please enter a password."));
    const PasswordHasher quick(qMin(kSampleIterations, m_hasher.iterations()));
    return createUser(fullName, email, password, Role::Customer, quick, false);
}

Result<Session> AuthService::createUser(const QString &fullName, const QString &email, const QString &password, Role role,
                                        const PasswordHasher &hasher, bool enforcePasswordRules)
{
    const QString name = fullName.trimmed();
    if (auto st = validation::checkRequired(name, QStringLiteral("Full name"), 100); !st)
        return st;
    const QString normalized = validation::normalizeEmail(email);
    if (!validation::isValidEmail(normalized))
        return Result<Session>::fail(QStringLiteral("Please enter a valid email address."));
    if (enforcePasswordRules) {
        if (auto st = validation::checkPassword(password); !st)
            return st;
    }

    auto existing = m_repos.users.findByEmail(normalized);
    if (!existing)
        return existing.status();
    if (existing.value())
        return Result<Session>::fail(QStringLiteral("An account with this email already exists."));

    auto id = m_repos.users.insert(normalized, name, hasher.hash(password), role);
    if (!id)
        return id.status();

    m_repos.audit.log(id.value(), role == Role::Admin ? QStringLiteral("admin_created") : QStringLiteral("register"),
                      QStringLiteral("user"), id.value());
    qInfo() << "Account created, id" << id.value() << "role" << roleToDb(role);
    return Session(id.value(), name, normalized, role);
}

Status AuthService::changePassword(const Session &session, const QString &currentPassword, const QString &newPassword)
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.users.findByEmail(session.email());
    if (!found)
        return found.status();
    if (!found.value())
        return Status::fail(QStringLiteral("Account not found."));
    if (!m_hasher.verify(currentPassword, found.value()->passwordHash))
        return Status::fail(QStringLiteral("Your current password is incorrect."));
    if (auto st = validation::checkPassword(newPassword); !st)
        return st;
    if (currentPassword == newPassword)
        return Status::fail(QStringLiteral("The new password must be different from the current one."));

    if (auto st = m_repos.users.updatePasswordHash(session.userId(), m_hasher.hash(newPassword)); !st)
        return st;
    m_repos.audit.log(session.userId(), QStringLiteral("password_changed"), QStringLiteral("user"), session.userId());
    return Status::ok();
}

void AuthService::logout(const Session &session)
{
    if (session.isValid())
        m_repos.audit.log(session.userId(), QStringLiteral("logout"), QStringLiteral("user"), session.userId());
}

Result<bool> AuthService::hasAdmin() const
{
    auto count = m_repos.users.countByRole(Role::Admin);
    if (!count)
        return count.status();
    return count.value() > 0;
}

} // namespace shophub
