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
sample_file = Path("kunit_test.c")
output_dir = Path("generated_tests")
output_dir.mkdir(parents=True, exist_ok=True)

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


# ----------------------
# Read sample KUnit test for context
# ----------------------
# The sample file provides a reference for the desired output format.
print("Reading sample KUnit test file...")
sample_code = sample_file.read_text(encoding="utf-8") if sample_file.exists() else "// No sample reference available"
if "No sample" in sample_code:
    print(f"Warning: Sample file '{sample_file}' not found.")


# ----------------------
# Main loop to generate tests
# ----------------------
# Iterate over each C function file in the specified directory.
for func_file in functions_dir.glob("*.c"):
    func_code = func_file.read_text(encoding="utf-8")

    # Construct the detailed prompt for the model
    prompt = f"""
You are an expert C programmer specializing in writing robust KUnit tests for the Linux kernel. Your task is to generate a complete, self-contained, and compilable C test file that avoids common compilation errors like `-Werror=unused-function`.

**Function to Test:**
* **Name**: {{func_name}}
* **File**: {{func_file}}
* **Code**:
    ```c
    {{func_code}}
    ```

**Instructions:**
Your primary goal is to generate a test that correctly isolates the function under test. Follow this precise pattern:

1.  **Include Headers:** Start with `#include <kunit/test.h>` and any other headers required for data types.
2.  **Define Mock Structs:** Define minimal versions of any structs needed for the function's parameters or logic. Only include the fields that are actually used.
3.  **Mock Dependencies via Preprocessor (`#define`):**
    * Analyze the function to be tested and identify all external functions it calls.
    * For each dependency, create a `static` mock implementation (e.g., `my_mock_function`).
    * **Crucially**, use the preprocessor to redirect the original function call to your mock **before** including the source file. For example:
        ```c
        #define function_to_mock(...) mock_function_implementation(__VA_ARGS__)
        ```
4.  **Exception for Inline Data Accessors:**
    * Many kernel functions (like `pinctrl_dev_get_drvdata`) are simple `static inline` functions or macros that just access a struct member.
    * For these specific functions, **DO NOT** create a mock function or use `#define`. This is the most common cause of `-Wunused-function` errors.
    * Instead, directly manipulate the data structures in your test's "Arrange" step to control the behavior. For `pinctrl_dev_get_drvdata`, you will set the `driver_data` field on your test `pinctrl_dev` struct.
5.  **Include the Source `.c` File:**
    * **After** all your `#define` redirects and mock implementations are in place, include the target C file directly. This ensures the C file is compiled using your mocks.
        ```c
        #include "path/to/the/file_to_test.c"
        ```
6.  **Write Test Cases (Arrange, Act, Assert):**
    * **Arrange:** Set up all test data. Use `kunit_kzalloc` to create instances of structs. Configure their fields to control the test's logic. If you are testing a data accessor, this is where you set the relevant struct members.
    * **Act:** Call the function under test.
    * **Assert:** Use KUnit macros like `KUNIT_EXPECT_EQ` to verify the outcome.
7.  **Create Test Suite:** Define the `kunit_case` array and the `kunit_suite` to register your test.

**Output Requirement:**
Generate **only the raw C code** for the test file. Do not include any introductory text, explanations, or markdown formatting like ````c` or ````.
"""


    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
