
	// ============================================================================
	// Python Errors (9100-9199)
	// ============================================================================

	/** @brief Python is not available or not initialized */
	constexpr const TCHAR* PYTHON_NOT_AVAILABLE = TEXT("PYTHON_NOT_AVAILABLE");

	/** @brief Python syntax error in code */
	constexpr const TCHAR* PYTHON_SYNTAX_ERROR = TEXT("PYTHON_SYNTAX_ERROR");

	/** @brief Python runtime error or exception */
	constexpr const TCHAR* PYTHON_RUNTIME_ERROR = TEXT("PYTHON_RUNTIME_ERROR");

	/** @brief Python execution exceeded timeout */
	constexpr const TCHAR* PYTHON_EXECUTION_TIMEOUT = TEXT("PYTHON_EXECUTION_TIMEOUT");

	/** @brief Python module not found */
	constexpr const TCHAR* PYTHON_MODULE_NOT_FOUND = TEXT("PYTHON_MODULE_NOT_FOUND");

	/** @brief Python class not found in module */
	constexpr const TCHAR* PYTHON_CLASS_NOT_FOUND = TEXT("PYTHON_CLASS_NOT_FOUND");

	/** @brief Python function not found in module */
	constexpr const TCHAR* PYTHON_FUNCTION_NOT_FOUND = TEXT("PYTHON_FUNCTION_NOT_FOUND");

	/** @brief Python introspection failed */
	constexpr const TCHAR* PYTHON_INTROSPECTION_FAILED = TEXT("PYTHON_INTROSPECTION_FAILED");

	/** @brief Unsafe Python code detected */
	constexpr const TCHAR* PYTHON_UNSAFE_CODE = TEXT("PYTHON_UNSAFE_CODE");

	/** @brief Invalid Python expression */
	constexpr const TCHAR* PYTHON_INVALID_EXPRESSION = TEXT("PYTHON_INVALID_EXPRESSION");

} // namespace ErrorCodes
} // namespace VibeUE
