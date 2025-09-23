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

    # Construct the detailed prompt for the model
    prompt = f"""
You are an expert Linux kernel developer with a specialization in writing high-quality, compilable KUnit tests.

## Goal
Your task is to write a single, complete, and compilable KUnit test file named `generated_kunit_test.c`. This file must provide test coverage for all C functions provided in the input.

## Inputs

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

3.  **Previous Errors to Fix (`error_logs`)**: A log of compilation errors from previous attempts. Your generated code must not repeat these errors.
    ```
    {error_logs}
    ```

## Strict Instructions

1.  **Analyze and Plan**:
    * First, thoroughly analyze all provided inputs. Your primary goal is to generate code that does not repeat any of the errors found in `{error_logs}`.
    * Identify every function within the `{func_code}` block. For each function you find, you must plan a dedicated test case. Name the test case `test_<original_function_name>`.

2.  **Implement Test Cases**:
    * Write the C code for each planned test case.
    * Strictly adhere to the coding style, macro usage, and overall structure of the `{sample_code2}` reference file.
    * Copy every function from `{func_code}` into the test file and declare it as `static`. This is required to make them directly callable by the tests.
    * Define only the minimal mock structs and helper functions necessary for the tests. Do not generate any code that results in `-Wunused-function` or `-Wunused-variable` warnings.

3.  **Consolidate into a Single File**:
    * Combine all generated `KUNIT_CASE` definitions into a single `kunit_case` array.
    * Define a single `kunit_suite` that uses this array.
    * Register the suite using `kunit_test_suite()`.

## Output Format
Produce **only the raw C code** for the `generated_kunit_test.c` file. Do not include any explanations, markdown like ` ```c `, placeholders, or any text other than the valid C code itself.
"""

    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
