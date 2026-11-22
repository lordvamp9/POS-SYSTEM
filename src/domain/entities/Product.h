#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace vamp9::pos::domain {

// Exact integer monetary representation (minor units / cents) to prevent IEEE-754 drift.
using MoneyCents = std::int64_t;

// Tax rate expressed in basis points (e.g., 2100 = 21.00%, 1050 = 10.50%).
using TaxRateBps = std::int32_t;

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotFound,
    Conflict,
    DatabaseError,
    HardwareError,
    SyncError,
    NotImplemented
};

struct Error {
    ErrorCode code{ErrorCode::None};
    std::string message;
};

// Lightweight C++20 Result<T> (std::expected-style) for explicit, exception-free error propagation.
template <typename T>
class Result {
public:
    static Result ok(T value) {
        return Result(std::move(value));
    }

    static Result fail(Error error) {
        return Result(std::move(error));
    }

    static Result fail(ErrorCode code, std::string message) {
        return Result(Error{code, std::move(message)});
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] const T& value() const & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T& value() & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] const Error& error() const & {
        return std::get<Error>(storage_);
    }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
};

template <>
class Result<void> {
public:
    static Result ok() {
        return Result();
    }

    static Result fail(Error error) {
        return Result(std::move(error));
    }

    static Result fail(ErrorCode code, std::string message) {
        return Result(Error{code, std::move(message)});
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return !hasError_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] const Error& error() const & noexcept {
        return error_;
    }

private:
    Result() = default;
    explicit Result(Error error) : hasError_(true), error_(std::move(error)) {}

    bool hasError_{false};
    Error error_{};
};

struct Product {
    std::int64_t id{0};
    std::string sku;
    std::string barcode;
    std::string name;
    std::string category{"General"};
    MoneyCents unitPriceCents{0};
    TaxRateBps taxRateBps{0};
    std::int32_t stockQuantity{0};
    bool isActive{true};
    std::string createdAtIso;
    std::string updatedAtIso;
};

} // namespace vamp9::pos::domain
