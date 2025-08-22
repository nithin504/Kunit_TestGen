# main.py
import os
from pathlib import Path
from dotenv import load_dotenv
import sys
import torch
from transformers import AutoTokenizer, AutoModelForCausalLM

# --- LangChain Imports ---
# NOTE: You may need to install langchain-community: pip install langchain-community
from langchain.prompts import PromptTemplate
from langchain.chains import LLMChain
from langchain_community.llms import HuggingFaceLLM

# --- Configuration ---

# Define file and directory paths
ROOT_DIR = Path(__file__).parent
FUNCTIONS_DIR = ROOT_DIR / "functions_extract"
OUTPUT_DIR = ROOT_DIR / "generated_tests"
SAMPLE_FILE = ROOT_DIR / "kunit_test.c"
ERROR_LOG_FILE = ROOT_DIR / "kunit_test_errors.txt"

# --- Initialization ---

def setup_paths():
    """Creates the output directory if it doesn't exist."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"📁 Output directory is: {OUTPUT_DIR}")
    if not FUNCTIONS_DIR.exists():
        print(f"❌ Error: Source functions directory not found at '{FUNCTIONS_DIR}'")
        sys.exit(1) # Exit if the source directory is missing

# --- Model Interaction ---

def load_starcoder_model():
    """
    Loads the StarCoder-2 model and wraps it for use with LangChain.
    """
    model_id = "bigcode/starcoder2-15b"
    print(f"--- Loading Hugging Face Model: {model_id} ---")
    print("⚠️  This is a large model and may take a while to download and load.")
    print("Ensure you have a powerful GPU with sufficient VRAM (>24GB recommended).")

    try:
        tokenizer = AutoTokenizer.from_pretrained(model_id)
        model = AutoModelForCausalLM.from_pretrained(
            model_id,
            torch_dtype=torch.bfloat16,
            device_map="auto",
        )

        # Define model keyword arguments for generation
        model_kwargs = {
            "max_new_tokens": 4096,
            "temperature": 0.2,
            "repetition_penalty": 1.1,
        }

        # Wrap the local model and tokenizer in the LangChain LLM
        # This removes the need for the transformers.pipeline
        llm = HuggingFaceLLM(
            model=model,
            tokenizer=tokenizer,
            model_kwargs=model_kwargs,
        )
        
        print("✅ Model loaded and wrapped for LangChain.")
        return llm

    except Exception as e:
        print(f"❌ An error occurred while loading the model: {e}")
        print("Please check your hardware compatibility and available memory.")
        sys.exit(1)

def create_prompt_template() -> str:
    """Constructs the prompt template string for the code generation model."""
    return """
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
    {sample_code}
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
    * Strictly adhere to the coding style, macro usage, and overall structure of the `{sample_code}` reference file.
    * Copy every function from `{func_code}` into the test file and declare it as `static`. This is required to make them directly callable by the tests.
    * Define only the minimal mock structs and helper functions necessary for the tests. Do not generate any code that results in `-Wunused-function` or `-Wunused-variable` warnings.

3.  **Consolidate into a Single File**:
    * Combine all generated `KUNIT_CASE` definitions into a single `kunit_case` array.
    * Define a single `kunit_suite` that uses this array.
    * Register the suite using `kunit_test_suite()`.

## Output Format
Produce **only the raw C code** for the `generated_kunit_test.c` file. Your entire output must be valid C code. Do not include any explanations, markdown like ` ```c `, placeholders, or any text other than the code itself.
"""

# --- Main Logic ---

def process_file(func_file: Path, llm_chain: LLMChain, sample_code: str, error_logs: str):
    """Processes a single C function file to generate a KUnit test using a LangChain chain."""
    print(f"🔹 Processing {func_file.name}...")
    func_code = func_file.read_text(encoding="utf-8")

    # The model often repeats the prompt back. We pass the prompt template to strip it.
    prompt_text = llm_chain.prompt.format(
        func_code=func_code,
        sample_code=sample_code,
        error_logs=error_logs
    )

    # Run the LangChain chain with the provided inputs
    full_response = llm_chain.run(
        func_code=func_code,
        sample_code=sample_code,
        error_logs=error_logs
    )

    # The raw response includes the prompt, so we strip it to get only the generated code
    response_text = full_response.replace(prompt_text, "").strip()

    # Clean up the response to ensure it's raw code
    if "```c" in response_text:
        response_text = response_text.split("```c\n")[1].split("```")[0]

    out_file = OUTPUT_DIR / f"{func_file.stem}_kunit_test.c"
    out_file.write_text(response_text, encoding="utf-8")
    print(f"✅ Test generated and saved to: {out_file}")

def main():
    """Main function to orchestrate the test generation process."""
    print("--- Starting KUnit Test Generation with StarCoder-2 and LangChain ---")
    setup_paths()

    # Load the local StarCoder-2 model as a LangChain LLM
    llm = load_starcoder_model()

    # Create the prompt structure using LangChain's PromptTemplate
    prompt_template = create_prompt_template()
    prompt = PromptTemplate(
        template=prompt_template,
        input_variables=["func_code", "sample_code", "error_logs"]
    )

    # Create the LangChain chain
    llm_chain = LLMChain(prompt=prompt, llm=llm)
    print("🔗 LangChain LLMChain created.")

    # Read context files
    print("📚 Reading sample KUnit test and error log files...")
    sample_code = SAMPLE_FILE.read_text(encoding="utf-8") if SAMPLE_FILE.exists() else "// No sample reference available"
    error_logs = ERROR_LOG_FILE.read_text(encoding="utf-8") if ERROR_LOG_FILE.exists() else "// No error logs available"

    if "No sample" in sample_code:
        print(f"⚠️  Warning: Sample file '{SAMPLE_FILE}' not found.")
    if "No error logs" in error_logs:
        print(f"ℹ️  Info: Error log file '{ERROR_LOG_FILE}' not found. Proceeding without previous error context.")

    # Find and process C files
    c_files = list(FUNCTIONS_DIR.glob("*.c"))
    if not c_files:
        print(f"❌ No C files found in {FUNCTIONS_DIR}. Nothing to do.")
        return
        
    print(f"\nFound {len(c_files)} C file(s) to process.")
    for func_file in c_files:
        process_file(func_file, llm_chain, sample_code, error_logs)

    print("\n🎉 All tests have been generated.")

if __name__ == "__main__":
    main()
