kunit_generation_prompt="""
You are an expert Linux kernel developer with deep experience in writing **high-quality, coverage-focused KUnit tests**.

Your task is to write a single, complete, and compilable KUnit test file . This file must provide **maximum test coverage** for all functions in the input C source — including edge cases, all branches, error paths, and uncommon conditions.

1.  **Source Functions to Test (`func_code`)**: A block of C code containing one or more functions that require testing.
    ```c
    {func_code}
    ```

2.  **Reference KUnit Test (`sample_code`)**: An example KUnit test file to be used for style, structure, and formatting.
    ```c
    {sample_code1}
    ```

    **Reference 2:**
    ```c
    {sample_code2}
    ```

    **Reference 3:**
    ```c
    {sample_code3}
    ```

3.  **Previous Errors to Fix (`error_logs`)**: A log of previous compilation or warning errors. Your generated code must not repeat these errors.
    ```
    {error_logs}
    ```

## Critical Rules to Avoid Compilation Errors

Based on the `error_logs`, you **MUST** adhere to the following rules:

1.  **Correct `kunit_kzalloc` Usage**: When calling `kunit_kzalloc`, you **must** provide all three required arguments: `(struct kunit *test, size_t size, gfp_t gfp)`. The third argument, the memory allocation flag, should almost always be `GFP_KERNEL`.
    - **Incorrect**: `kunit_kzalloc(test, sizeof(*data))`
    - **Correct**: `kunit_kzalloc(test, sizeof(*data), GFP_KERNEL)`

2.  **Correct `kunit_suite` Structure**: The member name for the array of test cases within `struct kunit_suite` is `test_cases`, **not** `cases`.
    - **Incorrect**: `.cases = your_test_case_array,`
    - **Correct**: `.test_cases = your_test_case_array,`

3.  **Correct `kunit_test_suite` Macro Call**: The `kunit_test_suite()` macro takes the suite struct object directly, **not a pointer** to it.
    - **Incorrect**: `kunit_test_suite(&my_suite);`
    - **Correct**: `kunit_test_suite(my_suite);`


## Instructions

### 1. Analyze and Plan
- Carefully analyze **every function** in `{func_code}`.
- Plan test cases that **cover all control paths**:
  - All `if`/`else` branches
  - All return paths
  - Invalid or unusual inputs
  - Edge conditions (e.g. `NULL`, `0`, `-1`, `INT_MAX`, etc.)
- Name each test case `test_<function_name>_<condition>` (e.g., `test_parse_header_invalid`).

### 2. Implement KUnit Test Cases
- For every identified function, implement multiple test cases as needed to **achieve full code coverage**.
- Ensure each test uses appropriate `KUNIT_EXPECT_*` macros and checks return values, outputs, or side effects.
- Strictly follow the **style and layout** of `{sample_code2}`.

### 3. Code Integration
- Copy all tested functions from `{func_code}` into the test file and mark them `static` to make them directly callable.
- Define only **minimal mock structs** or test helpers. Avoid unused code and suppress `-Wunused-*` warnings.

### 4. Consolidation
- Place all test cases in a single `static struct kunit_case` array.
- Define one `static struct kunit_suite` that references this array using `.test_cases`.
- Use `kunit_test_suite()` to register the test suite.

## Output Requirements

- Output only the **pure C source code** of the generated file `generated_kunit_test.c`.
- Do **not** include:
  - Markdown (e.g., ```c)
  - Placeholders
  - Comments
  - Explanations
- The output must be **compilable**, **style-compliant**, and **warning-free**, strictly following the **Critical Rules** above.
"""
