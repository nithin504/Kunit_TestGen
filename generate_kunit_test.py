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
error_log_file = Path("kunit_test.txt")
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
error_logs = error_log_file.read_text(encoding="utf-8") if error_log_file.exists() else "// No error logs available"

if "No sample" in sample_code:
    print(f"⚠️  Warning: Sample file '{sample_file}' not found.")
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
You are an expert Linux kernel developer skilled in writing KUnit tests.

## Functions to Test
Each `.c` file in the `functions_extract/` folder contains one or more functions.
Here is the combined source content you must cover:
```c
{func_code}

Reference KUnit Test (for style/format)
{sample_code}

Previous Errors to Avoid
{error_logs}

Requirements:

Automatically detect all function names from the provided code and generate dedicated test cases for each one.

Follow the exact structure, macros, and conventions from kunit_test.c.

Fix issues similar to those listed in the provided error log.

Define only minimal mocks/structs needed. Do not add unused variables or functions.

Copy every function under test into the file as static so they can be tested directly.

Generate a single compilable KUnit test file named generated_kunit_test.c containing:

All provided functions

Test cases for each function

A single kunit_case[] array, kunit_suite, and kunit_test_suite

Output must be only valid C code (no explanations, no markdown, no placeholders).

Output:

Return the full content of a single C file generated_kunit_test.c that contains KUnit tests for all functions.
"""

    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
