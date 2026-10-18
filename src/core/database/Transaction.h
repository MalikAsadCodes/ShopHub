#pragma once

#include "common/Result.h"

#include <QString>

namespace shophub {

// RAII write transaction (BEGIN IMMEDIATE). If commit() is not reached,
// the destructor rolls everything back, so a failed checkout never leaves
// half an order behind.
class Transaction
{
public:
    explicit Transaction(QString connectionName);
    ~Transaction();

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    Status begin();
    Status commit();
    void rollback();
    bool isActive() const { return m_active; }

private:
    QString m_connection;
    bool m_active = false;
};

} // namespace shophub
