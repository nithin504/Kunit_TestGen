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
You are an expert C programmer tasked with writing a complete and error-free KUnit test for a static Linux kernel function.

**Your Task:**
Write a complete, self-contained, and compilable KUnit test file for the C function provided below.

**Function to Test:**
```c
// PASTE THE ENTIRE C FUNCTION YOU WANT TO TEST HERE
static int amd_get_groups_count(struct pinctrl_dev *pctldev)
{{
	struct amd_gpio *gpio_dev = pinctrl_dev_get_drvdata(pctldev);

	return gpio_dev->ngroups;
}}
````

**Strict Requirements for the Generated Code:**

1.  **Identify the Function Name:** Automatically identify the name of the function from the code pasted above (e.g., `amd_get_groups_count`).
2.  **No Unused Code:** Do **not** generate any helper functions, mock functions, or variables that are not actually called or used. The final code must compile without any `-Wunused-function` or `-Wunused-variable` warnings.
3.  **Correct Data Structures:** Define minimal mock versions of any necessary structs (like `struct amd_gpio`). Only include fields that are actually accessed by the function under test.
4.  **Handle Data Accessors Correctly:** If the function uses a simple data accessor macro or inline function (like `pinctrl_dev_get_drvdata`), **do not** create a mock for it. Instead, set the data it accesses directly in your test setup (e.g., `pctldev->driver_data = ...`).
5.  **Include the Function:** The function being tested must be included in the test file so it can be called. You can do this by copying the function's code directly into the test file and marking it `static`.
6.  **Complete Test Suite:** The final output must be a single, complete file containing the test case and the KUnit suite definition (`kunit_case`, `kunit_suite`, `kunit_test_suite`).
7.  **No Placeholders:** Do not use any placeholders like `{{func_name}}` in the final output. The code must be ready to compile immediately.

**Output Requirement:**
Generate only the raw C code. Do not include any explanations or markdown formatting like ` ```c `.

"""

    print(f"🔹 Generating test for {func_file.name}...")
    response_text = query_model(prompt)

    # Define the output file path
    out_file = output_dir / f"{func_file.stem}_kunit_test.c"
    # Write the generated code to the output file
    out_file.write_text(response_text, encoding="utf-8")

    print(f"✅ Test generated and saved to: {out_file}")

print("\nAll tests have been generated.")
