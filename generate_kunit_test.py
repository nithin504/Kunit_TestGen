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
sample_file = Path("test_kunit.c")
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
You are an expert C programmer writing a KUnit test for a Linux kernel function. Your task is to generate a complete, self-contained, and compilable C test file.

**Function to Test:**
{func_code}


**Instructions:**
To properly unit test the function, you must isolate it from its dependencies. Follow this exact pattern:
1.  **Include Headers:** Start with `#include <kunit/test.h>` and any other necessary headers.
2.  **Define Mock Structs:** Define any necessary structs, like `struct amd_gpio`, that are needed for the test.
3.  **Mock Dependencies:** The function under test calls `pinctrl_dev_get_drvdata`. You MUST provide a mock implementation of this function within the test file. This mock should return a pointer to a test-controlled struct.
    
    // Example mock
    static void *pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev) {{
        return pctldev->dev.driver_data;
    }}
    
4.  **Copy the Function Under Test:** Copy the exact function to be tested (`{func_file.stem}`) into the test file and declare it as `static`. This ensures the test calls the local version, which will then use your mock dependency.
5.  **Write the Test Case:** Create a static void KUnit test case function (e.g., `static void test_my_function(struct kunit *test)`).
6.  **Setup Test Data:** Inside the test case, create an instance of the required structs (e.g., `struct amd_gpio`, `struct pinctrl_dev`). Use `kunit_kzalloc` for memory allocation. Set the `driver_data` of your test `pinctrl_dev` to point to your mock `amd_gpio` struct.
7.  **Call and Assert:** Call the function under test with your prepared test data and use `KUNIT_EXPECT_EQ` to verify that the return value is correct.
8.  **Create Test Suite:** Define the `kunit_case` array and the `kunit_test_suite` to register the test.

**Reference KUnit Structure:**

{sample_code}


**Output Requirement:**
Generate **only the complete C code** for the test file. Do not include any explanations or markdown.
"""


    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
