#pragma once

#include "CoreMinimal.h"

/**
 * Type-safe Result wrapper for operations that can succeed or fail.
 * Provides compile-time safety to replace raw JSON responses.
 * 
 * @tparam T The type of the success value
 * 
 * Example usage:
 * @code
 * TResult<int32> Divide(int32 A, int32 B)
 * {
 *     if (B == 0)
 *     {
 *         return TResult<int32>::Error(TEXT("DIVISION_BY_ZERO"), TEXT("Cannot divide by zero"));
 *     }
 *     return TResult<int32>::Success(A / B);
 * }
 * 
 * TResult<int32> Result = Divide(10, 2);
 * if (Result.IsSuccess())
 * {
 *     UE_LOG(LogTemp, Log, TEXT("Result: %d"), Result.GetValue());
 * }
 * else
 * {
 *     UE_LOG(LogTemp, Error, TEXT("Error [%s]: %s"), 
 *            *Result.GetErrorCode(), *Result.GetErrorMessage());
 * }
 * @endcode
 */
template<typename T>
class VIBEUE_API TResult
{
public:
    /**
     * Creates a successful result with the given value (copy).
     * 
     * @param Value The success value to store
     * @return A successful TResult containing the value
     */
    static TResult Success(const T& Value)
    {
        return TResult(true, Value, FString(), FString());
    }

    /**
     * Creates a successful result with the given value (move).
     * 
     * @param Value The success value to store (will be moved)
     * @return A successful TResult containing the value
     */
    static TResult Success(T&& Value)
    {
        return TResult(true, MoveTemp(Value), FString(), FString());
    }

    /**
     * Creates an error result with the given error code and message.
     * 
     * @param ErrorCode A machine-readable error code (e.g., "PARAM_INVALID")
     * @param ErrorMessage A human-readable error message
     * @return An error TResult with the specified error information
     */
    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, T(), ErrorCode, ErrorMessage);
    }

    /**
     * Checks if this result represents a success.
     * 
     * @return true if the result is successful, false otherwise
     */
    bool IsSuccess() const { return bSuccess; }

    /**
     * Checks if this result represents an error.
     * 
     * @return true if the result is an error, false otherwise
     */
    bool IsError() const { return !bSuccess; }
    
    /**
     * Gets the success value.
     * 
     * @return The success value
     * @note This will trigger a check() assertion if called on an error result
     */
    const T& GetValue() const 
    { 
        check(bSuccess); 
        return Value; 
    }
    
    /**
     * Gets the error code.
     * 
     * @return The error code string, or empty string if this is a success result
     */
    const FString& GetErrorCode() const { return ErrorCode; }

    /**
     * Gets the error message.
     * 
     * @return The error message string, or empty string if this is a success result
     */
    const FString& GetErrorMessage() const { return ErrorMessage; }

    /**
     * Maps the success value to a new type using the provided function.
     * If this result is an error, the error is propagated to the new result.
     * 
     * @tparam U The type of the mapped value
     * @param Fn Function to transform the success value
     * @return A new TResult with the transformed value, or the same error
     * 
     * Example:
     * @code
     * TResult<int32> IntResult = TResult<int32>::Success(42);
     * TResult<FString> StringResult = IntResult.Map([](int32 Val) {
     *     return FString::Printf(TEXT("%d"), Val);
     * });
     * @endcode
     */
    template<typename U>
    TResult<U> Map(TFunction<U(const T&)> Fn) const
    {
        if (IsSuccess())
        {
            return TResult<U>::Success(Fn(Value));
        }
        return TResult<U>::Error(ErrorCode, ErrorMessage);
    }

    /**
     * Flat-maps the success value to a new result using the provided function.
     * If this result is an error, the error is propagated to the new result.
     * 
     * @tparam U The type of the mapped result value
     * @param Fn Function that takes the success value and returns a new TResult
     * @return The result of applying Fn to the success value, or the same error
     * 
     * Example:
     * @code
     * TResult<int32> IntResult = TResult<int32>::Success(42);
     * TResult<FString> StringResult = IntResult.FlatMap([](int32 Val) {
     *     if (Val > 0)
     *     {
     *         return TResult<FString>::Success(FString::Printf(TEXT("%d"), Val));
     *     }
     *     return TResult<FString>::Error(TEXT("INVALID"), TEXT("Value must be positive"));
     * });
     * @endcode
     */
    template<typename U>
    TResult<U> FlatMap(TFunction<TResult<U>(const T&)> Fn) const
    {
        if (IsSuccess())
        {
            return Fn(Value);
        }
        return TResult<U>::Error(ErrorCode, ErrorMessage);
    }

private:
    /**
     * Private constructor for copy semantics.
     * 
     * @param bInSuccess Whether this result represents success
     * @param InValue The success value
     * @param InErrorCode The error code
     * @param InErrorMessage The error message
     */
    TResult(bool bInSuccess, const T& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(InValue), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    /**
     * Private constructor for move semantics.
     * 
     * @param bInSuccess Whether this result represents success
     * @param InValue The success value (will be moved)
     * @param InErrorCode The error code
     * @param InErrorMessage The error message
     */
    TResult(bool bInSuccess, T&& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(MoveTemp(InValue)), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    T Value;
    FString ErrorCode;
    FString ErrorMessage;
};

/**
 * Specialization of TResult for void operations.
 * Used for operations that don't return a value but can still fail.
 * 
 * Example usage:
 * @code
 * TResult<void> DeleteFile(const FString& FilePath)
 * {
 *     if (FilePath.IsEmpty())
 *     {
 *         return TResult<void>::Error(TEXT("PARAM_INVALID"), TEXT("File path cannot be empty"));
 *     }
 *     
 *     // Perform deletion...
 *     return TResult<void>::Success();
 * }
 * 
 * TResult<void> Result = DeleteFile(TEXT("MyFile.txt"));
 * if (Result.IsError())
 * {
 *     UE_LOG(LogTemp, Error, TEXT("Failed to delete file: %s"), *Result.GetErrorMessage());
 * }
 * @endcode
 */
template<>
class VIBEUE_API TResult<void>
{
public:
    /**
     * Creates a successful void result.
     * 
     * @return A successful TResult<void>
     */
    static TResult Success()
    {
        return TResult(true, FString(), FString());
    }

    /**
     * Creates an error result with the given error code and message.
     * 
     * @param ErrorCode A machine-readable error code (e.g., "PARAM_INVALID")
     * @param ErrorMessage A human-readable error message
     * @return An error TResult<void> with the specified error information
     */
    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, ErrorCode, ErrorMessage);
    }

    /**
     * Checks if this result represents a success.
     * 
     * @return true if the result is successful, false otherwise
     */
    bool IsSuccess() const { return bSuccess; }

    /**
     * Checks if this result represents an error.
     * 
     * @return true if the result is an error, false otherwise
     */
    bool IsError() const { return !bSuccess; }
    
    /**
     * Gets the error code.
     * 
     * @return The error code string, or empty string if this is a success result
     */
    const FString& GetErrorCode() const { return ErrorCode; }

    /**
     * Gets the error message.
     * 
     * @return The error message string, or empty string if this is a success result
     */
    const FString& GetErrorMessage() const { return ErrorMessage; }

private:
    /**
     * Private constructor.
     * 
     * @param bInSuccess Whether this result represents success
     * @param InErrorCode The error code
     * @param InErrorMessage The error message
     */
    TResult(bool bInSuccess, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    FString ErrorCode;
    FString ErrorMessage;
};
