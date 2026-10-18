#include "repositories/CategoryRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

const QString kSelect = QStringLiteral(
    "SELECT c.id, c.name, c.is_active, "
    "  (SELECT COUNT(*) FROM products p WHERE p.category_id = c.id AND p.is_active = 1) "
    "FROM categories c ");

Category readCategory(const QSqlQuery &q)
{
    Category c;
    c.id = q.value(0).toLongLong();
    c.name = q.value(1).toString();
    c.active = q.value(2).toInt() == 1;
    c.productCount = q.value(3).toInt();
    return c;
}

} // namespace

Result<QVector<Category>> CategoryRepository::list(bool includeInactive) const
{
    QSqlQuery q(db());
    q.prepare(kSelect + (includeInactive ? QString() : QStringLiteral("WHERE c.is_active = 1 "))
              + QStringLiteral("ORDER BY c.name COLLATE NOCASE"));
    if (auto st = sql::exec(q, "CategoryRepository::list"); !st)
        return st;
    QVector<Category> out;
    while (q.next())
        out.push_back(readCategory(q));
    return out;
}

Result<std::optional<Category>> CategoryRepository::findById(qint64 id) const
{
    QSqlQuery q(db());
    q.prepare(kSelect + QStringLiteral("WHERE c.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(q, "CategoryRepository::findById"); !st)
        return st;
    if (!q.next())
        return std::optional<Category>{};
    return std::optional<Category>{readCategory(q)};
}

Result<qint64> CategoryRepository::insert(const QString &name)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO categories (name, created_at) VALUES (:n, :c)"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":c"), sql::nowUtc());
    if (!q.exec()) {
        if (sql::isUniqueViolation(q))
            return Result<qint64>::fail(QStringLiteral("A category with this name already exists."));
        return sql::failure(q, "CategoryRepository::insert");
    }
    return q.lastInsertId().toLongLong();
}

Status CategoryRepository::rename(qint64 id, const QString &name)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE categories SET name = :n WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        if (sql::isUniqueViolation(q))
            return Status::fail(QStringLiteral("A category with this name already exists."));
        return sql::failure(q, "CategoryRepository::rename");
    }
    return Status::ok();
}

Status CategoryRepository::setActive(qint64 id, bool active)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE categories SET is_active = :a WHERE id = :id"));
    q.bindValue(QStringLiteral(":a"), active ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "CategoryRepository::setActive");
}

} // namespace shophub
