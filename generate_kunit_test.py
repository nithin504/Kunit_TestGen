import os
from pathlib import Path
import google.generativeai as genai
from dotenv import load_dotenv

# ----------------------
# Load environment variables and configure API Key
# ----------------------
# Load variables from a .env file if it exists
load_dotenv() 

# Get the Google API key from environment variables
# Make sure you have a .env file with GOOGLE_API_KEY="YOUR_API_KEY"
# or have it set in your system's environment variables.
# If the error persists, also ensure your library is updated:
# pip install --upgrade google-generativeai
GOOGLE_API_KEY = os.environ.get("GOOGLE_API_KEY")
if not GOOGLE_API_KEY:
    raise ValueError("GOOGLE_API_KEY environment variable not set. Please create a .env file or set it.")

# Configure the generative AI client
genai.configure(api_key=GOOGLE_API_KEY)

# ----------------------
# Setup file paths  
# ----------------------
functions_dir = Path("functions_extract")
sample_file1 = Path("kunit_test.c")
sample_file2 = Path("kunit_test2.c")
sample_file3 = Path("kunit_test3.c")
output_dir = Path("generated_tests")
error_log_file = Path("kunit_test.txt")
output_dir.mkdir(parents=True, exist_ok=True)
'''
# ----------------------
# Setup Gemini Pro Model
# ----------------------
# Initialize the Gemini 1.5 Flash model, a current and efficient model.
model = genai.GenerativeModel('gemini-2.0-flash-lite')

# Configuration for the generation process
generation_config = genai.GenerationConfig(
    temperature=0.2,
    # Increased token limit to prevent incomplete code generation
    max_output_tokens=4096, 
)


# ----------------------
# Helper to query the Gemini model
# ----------------------
def query_model(prompt: str) -> str:
    """
    Sends a prompt to the configured Gemini model and returns the text response.

    Args:
        prompt: The input prompt string for the model.

    Returns:
        The generated text from the model as a string.
    """
    try:
        # Generate content using the model
        response = model.generate_content(prompt, generation_config=generation_config)
        # Return the text part of the response
        return response.text
    except Exception as e:
        # Handle potential API errors gracefully
        print(f"An error occurred while querying the model: {e}")
        return f"// Error generating test: {e}"
'''# ----------------------
# Setup OpenAI Client for OpenRouter
from openai import OpenAI

# ----------------------
# Setup OpenRouter Client
# ----------------------
client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key="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",
)

# ----------------------
# Configuration
# ----------------------
MODEL_NAME ="deepseek/deepseek-chat-v3.1:free"  # You can change to other Gemini variants
TEMPERATURE = 0.2


# ----------------------
# Helper to query the OpenRouter model
# ----------------------
def query_model(prompt: str) -> str:
    """
    Sends a prompt to the configured OpenRouter model and returns the text response.

    Args:
        prompt: The input prompt string for the model.

    Returns:
        The generated text from the model as a string.
    """
    try:
        completion = client.chat.completions.create(
            model=MODEL_NAME,
            messages=[
                {"role": "user", "content": prompt}
            ],
            temperature=TEMPERATURE,
            max_tokens=4096,  # Increased token limit to prevent incomplete code generation
            
        )
        return completion.choices[0].message.content
    except Exception as e:
        print(f"An error occurred while querying the model: {e}")
        return f"// Error generating response: {e}"
# ----------------------
# Read sample KUnit test for context
# ----------------------
# The sample file provides a reference for the desired output format.
print("Reading sample KUnit test file...")
sample_code1 = sample_file1.read_text(encoding="utf-8") if sample_file1.exists() else "// No sample reference available"
sample_code2 = sample_file2.read_text(encoding="utf-8") if sample_file2.exists() else "// No sample reference available"
sample_code3 = sample_file3.read_text(encoding="utf-8") if sample_file3.exists() else "// No sample reference available"

error_logs = error_log_file.read_text(encoding="utf-8") if error_log_file.exists() else "// No error logs available"

if "No sample" in sample_code1:
    print(f"⚠️  Warning: Sample file '{sample_file1}' not found.")
if "No sample" in sample_code2:
    print(f"⚠️  Warning: Sample file '{sample_file2}' not found.")
if "No sample" in sample_code3:
    print(f"⚠️  Warning: Sample file '{sample_file3}' not found.")
if "No error logs" in error_logs:
    print(f"⚠️  Warning: Error log file '{error_log_file}' not found.")


# ----------------------
# Main loop to generate tests
# ----------------------
# Iterate over each C function file in the specified directory.
for func_file in functions_dir.glob("*.c"):
    func_code = func_file.read_text(encoding="utf-8")

    # Construct the detailed prompt for the mode
    prompt = f"""
You are an expert Linux kernel developer with a specialization in writing high-quality, compilable, and high-coverage KUnit tests.

## Goal
Write a single, complete, and compilable KUnit test file named `generated_kunit_test.c`. The file must compile cleanly (no warnings/errors) and provide thorough test coverage for every C function provided in the inputs.

## Inputs

1. **Source Functions to Test (`func_code`)**
```c
{func_code}
````

<<<<<<< HEAD
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
=======
2. **Reference KUnit Test (`sample_code`)**
>>>>>>> 971a407db8f1a4efa72db35a0b684ae9deaacb9a

```c
{sample_code}
```

3. **Previous Errors to Fix (`error_logs`)**

```
{error_logs}
```

## Strict Instructions

1. **Analyze and Plan**

<<<<<<< HEAD
2.  **Implement Test Cases**:
    * Write the C code for each planned test case.
    * Strictly adhere to the coding style, macro usage, and overall structure of the `{sample_code2}` reference file.
    * Copy every function from `{func_code}` into the test file and declare it as `static`. This is required to make them directly callable by the tests.
    * Define only the minimal mock structs and helper functions necessary for the tests. Do not generate any code that results in `-Wunused-function` or `-Wunused-variable` warnings.
=======
   * Carefully read all inputs and explicitly avoid repeating any issue mentioned in `{error_logs}`.
   * Identify every top-level function defined in `{func_code}`. Create one dedicated test function per source function, named `test_<original_function_name>`.
>>>>>>> 971a407db8f1a4efa72db35a0b684ae9deaacb9a

2. **Function Integration**

   * Copy every function from `{func_code}` verbatim into the test file and mark it `static` (and `static inline` only if originally inline).
   * If prototypes are required, declare them `static` before use.
   * If any function is unused by tests after integration, either remove it or ensure at least one test invokes it to avoid `-Wunused-*` warnings. Do **not** leave dead code.

3. **Implement Test Cases**

   * For each source function, implement a matching KUnit test function using `KUNIT_EXPECT_*` / `KUNIT_ASSERT_*` macros.
   * Cover normal paths, edge cases, boundary values, invalid inputs, and error paths to maximize coverage.
   * Where functions depend on kernel types or external state, define only the **minimal** mock structs, fakes, or stubs needed for compilation and meaningful assertions.
   * Do not include any unused mocks, helpers, variables, or headers.

4. **Compilation Safety & Style**

   * Only include headers that are strictly necessary. Always include `<kunit/test.h>`. Add others (e.g., `<linux/slab.h>`, `<linux/string.h>`) **only if used**.
   * Ensure zero `-Wunused-function`, `-Wunused-variable`, or `-Wmissing-prototypes` warnings.
   * Follow the style, macros, and overall structure of `{sample_code}` exactly (naming, spacing, suite/case layout).
   * If a function is `static` and used only in tests, keep everything in the single file as required.

5. **Suite Organization**

   * Register each test with `KUNIT_CASE(test_<original_function_name>)`.
   * Collect all cases in a single `static struct kunit_case <suite_name>_cases[]` array.
   * Define a single `static struct kunit_suite <suite_name>` with `.name`, `.test_cases = <suite_name>_cases`.
   * Register the suite using `kunit_test_suite(<suite_name>);`.
   * Choose `<suite_name>` based on the source (e.g., `<basename>_kunit`), using only lowercase, digits, and underscores.

6. **Build Guards**

   * The file must be self-contained and compilable under `CONFIG_KUNIT`. Do not rely on external test utilities beyond what you define here and the standard KUnit API.

7. **Assertions & Helpers**

   * Prefer `KUNIT_ASSERT_*` for preconditions that make the rest of the test invalid, otherwise use `KUNIT_EXPECT_*`.
   * Use helper functions only if they are invoked by more than one test; otherwise inline logic to avoid unused symbols.

8. **No Red Flags**

   * Do not use APIs or headers that are unavailable in a minimal KUnit environment.
   * Do not introduce dynamic allocations unless required by the function under test; if used, free them and assert on success paths to avoid leaks.

## Consolidation into a Single File

* Place (a) the copied `static` source functions, (b) any minimal mocks/stubs, (c) all test functions, (d) the `struct kunit_case` array, (e) the `struct kunit_suite`, and (f) the `kunit_test_suite()` registration **all in this one file**.

## Output Format

Produce **only the raw C source code** for `generated_kunit_test.c`. Do not include explanations, markdown fences, placeholders, or any text other than valid C code.

## Objective

Output a **self-contained, warning-free, style-compliant KUnit test file** that maximizes code coverage for all provided functions while avoiding every issue recorded in `{error_logs}`.
"""




    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
