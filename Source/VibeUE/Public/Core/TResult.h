// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Result type for operations that can succeed or fail
 * Provides type safety and avoids runtime JSON parsing
 */
template<typename T>
class TResult
{
public:
    // Success constructor
    static TResult Success(const T& Value)
    {
        return TResult(true, Value, FString(), FString());
    }

    static TResult Success(T&& Value)
    {
        return TResult(true, MoveTemp(Value), FString(), FString());
    }

    // Error constructor with error code
    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, T(), ErrorCode, ErrorMessage);
    }

    // Accessors
    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }
    
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
    
    const FString& GetErrorCode() const { return ErrorCode; }
    const FString& GetErrorMessage() const { return ErrorMessage; }

private:
    TResult(bool bInSuccess, const T& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(InValue), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    TResult(bool bInSuccess, T&& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(MoveTemp(InValue)), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    T Value;
    FString ErrorCode;
    FString ErrorMessage;
};

// Specialization for bool
template<>
class TResult<bool>
{
public:
    static TResult Success(bool Value)
    {
        return TResult(true, Value, FString(), FString());
    }

    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, false, ErrorCode, ErrorMessage);
    }

    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }
    
    bool GetValue() const 
    { 
        check(bSuccess); 
        return Value; 
    }
    
    const FString& GetErrorCode() const { return ErrorCode; }
    const FString& GetErrorMessage() const { return ErrorMessage; }

private:
    TResult(bool bInSuccess, bool InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(InValue), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    bool Value;
    FString ErrorCode;
    FString ErrorMessage;
};

// Specialization for void
template<>
class TResult<void>
{
public:
    static TResult Success()
    {
        return TResult(true, FString(), FString());
    }

    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, ErrorCode, ErrorMessage);
    }

    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }
    
    const FString& GetErrorCode() const { return ErrorCode; }
    const FString& GetErrorMessage() const { return ErrorMessage; }

private:
    TResult(bool bInSuccess, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    FString ErrorCode;
    FString ErrorMessage;
};
