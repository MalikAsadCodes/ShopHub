#pragma once

#include <QString>
#include <optional>
#include <utility>

namespace shophub {

// Outcome of an operation that returns no value.
// Error messages are safe to show to users; technical details go to the log.
class Status
{
public:
    static Status ok() { return Status(); }
    static Status fail(QString message)
    {
        Status s;
        s.m_ok = false;
        s.m_error = std::move(message);
        return s;
    }

    bool isOk() const { return m_ok; }
    explicit operator bool() const { return m_ok; }
    const QString &error() const { return m_error; }

private:
    bool m_ok = true;
    QString m_error;
};

// Outcome of an operation that returns a value of type T on success.
template <typename T>
class Result
{
public:
    Result(T value) : m_value(std::move(value)) {} // NOLINT: implicit on purpose

    // Converts a failed Status into a failed Result.
    Result(const Status &failed) : m_error(failed.error()) {} // NOLINT

    static Result fail(QString message)
    {
        Result r;
        r.m_error = std::move(message);
        return r;
    }

    bool isOk() const { return m_value.has_value(); }
    explicit operator bool() const { return isOk(); }

    const T &value() const { return *m_value; }
    T &value() { return *m_value; }
    T takeValue() { return std::move(*m_value); }

    const QString &error() const { return m_error; }
    Status status() const { return isOk() ? Status::ok() : Status::fail(m_error); }

private:
    Result() = default;
    std::optional<T> m_value;
    QString m_error;
};

} // namespace shophub
