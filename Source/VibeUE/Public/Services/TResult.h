#pragma once

#include "CoreMinimal.h"

/**
 * Result type for Blueprint services
 * Provides type-safe error handling with either a value or an error message
 */
template<typename T>
class TResult
{
public:
    // Success constructor
    static TResult Success(T&& Value)
    {
        TResult Result;
        Result.bSuccess = true;
        Result.Value = MoveTemp(Value);
        return Result;
    }

    static TResult Success(const T& Value)
    {
        TResult Result;
        Result.bSuccess = true;
        Result.Value = Value;
        return Result;
    }

    // Error constructor
    static TResult Error(const FString& ErrorMessage)
    {
        TResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = ErrorMessage;
        return Result;
    }

    // Check if the result is successful
    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }

    // Get the value (only valid if IsSuccess() is true)
    const T& GetValue() const
    {
        check(bSuccess);
        return Value;
    }

    T& GetValue()
    {
        check(bSuccess);
        return Value;
    }

    // Get the error message (only valid if IsError() is true)
    const FString& GetError() const
    {
        check(!bSuccess);
        return ErrorMessage;
    }

    // Operator overloads for convenience
    explicit operator bool() const { return bSuccess; }

private:
    TResult() : bSuccess(false) {}

    bool bSuccess;
    T Value;
    FString ErrorMessage;
};

/**
 * Specialization for void return type
 */
template<>
class TResult<void>
{
public:
    // Success constructor
    static TResult Success()
    {
        TResult Result;
        Result.bSuccess = true;
        return Result;
    }

    // Error constructor
    static TResult Error(const FString& ErrorMessage)
    {
        TResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = ErrorMessage;
        return Result;
    }

    // Check if the result is successful
    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }

    // Get the error message (only valid if IsError() is true)
    const FString& GetError() const
    {
        check(!bSuccess);
        return ErrorMessage;
    }

    // Operator overloads for convenience
    explicit operator bool() const { return bSuccess; }

private:
    TResult() : bSuccess(false) {}

    bool bSuccess;
    FString ErrorMessage;
};
