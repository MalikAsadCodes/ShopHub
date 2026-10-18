#pragma once

#include <QString>
#include <QVector>
#include <optional>

namespace shophub {

// Order lifecycle:
//   Pending -> Confirmed -> Processing -> Shipped -> Delivered
//   Pending / Confirmed / Processing -> Cancelled
enum class OrderStatus { Pending, Confirmed, Processing, Shipped, Delivered, Cancelled };

namespace orderstatus {

QString toDb(OrderStatus s);                       // "pending"
std::optional<OrderStatus> fromDb(const QString &s);
QString displayName(OrderStatus s);                // "Pending"
QVector<OrderStatus> all();

bool canTransition(OrderStatus from, OrderStatus to);
// The forward step an administrator can take next (excludes cancellation).
std::optional<OrderStatus> nextForward(OrderStatus from);
bool isFinal(OrderStatus s);
bool customerCanCancel(OrderStatus s);
bool adminCanCancel(OrderStatus s);

} // namespace orderstatus
} // namespace shophub
