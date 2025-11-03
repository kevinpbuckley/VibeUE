// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Result type for service operations that can succeed or fail
 * Similar to Rust's Result<T, E> or other languages' Result types
 */
template<typename T>
class TResult
{
public:
    /** Construct a successful result with a value */
    static TResult<T> Ok(const T& InValue)
    {
        TResult<T> Result;
        Result.bSuccess = true;
        Result.Value = InValue;
        return Result;
    }

    /** Construct a successful result with a moved value */
    static TResult<T> Ok(T&& InValue)
    {
        TResult<T> Result;
        Result.bSuccess = true;
        Result.Value = MoveTemp(InValue);
        return Result;
    }

    /** Construct a failed result with an error message */
    static TResult<T> Err(const FString& InError)
    {
        TResult<T> Result;
        Result.bSuccess = false;
        Result.ErrorMessage = InError;
        return Result;
    }

    /** Check if the result is successful */
    bool IsOk() const { return bSuccess; }

    /** Check if the result is an error */
    bool IsError() const { return !bSuccess; }

    /** Get the value (only valid if IsOk() is true) */
    const T& GetValue() const
    {
        check(bSuccess);
        return Value;
    }

    /** Get the value (only valid if IsOk() is true) */
    T& GetValue()
    {
        check(bSuccess);
        return Value;
    }

    /** Get the error message (only valid if IsError() is true) */
    const FString& GetError() const
    {
        check(!bSuccess);
        return ErrorMessage;
    }

    /** Get the value or a default if error */
    T GetValueOr(const T& DefaultValue) const
    {
        return bSuccess ? Value : DefaultValue;
    }

private:
    bool bSuccess = false;
    T Value;
    FString ErrorMessage;
};

/**
 * Specialization for void type (for operations that don't return a value)
 */
template<>
class TResult<void>
{
public:
    /** Construct a successful result */
    static TResult<void> Ok()
    {
        TResult<void> Result;
        Result.bSuccess = true;
        return Result;
    }

    /** Construct a failed result with an error message */
    static TResult<void> Err(const FString& InError)
    {
        TResult<void> Result;
        Result.bSuccess = false;
        Result.ErrorMessage = InError;
        return Result;
    }

    /** Check if the result is successful */
    bool IsOk() const { return bSuccess; }

    /** Check if the result is an error */
    bool IsError() const { return !bSuccess; }

    /** Get the error message (only valid if IsError() is true) */
    const FString& GetError() const
    {
        check(!bSuccess);
        return ErrorMessage;
    }

private:
    bool bSuccess = false;
    FString ErrorMessage;
};
