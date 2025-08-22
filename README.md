AI-Powered KUnit Test Case Generator for C
git remote set-url origin https://Nithinkumar_cfRFstFheCIa0aKjYe4hyFlsgcZu9B43DDxM@github.com/nithin504/Kunit_testGen

Kunit_TestGen is a powerful command-line tool that automates the generation of KUnit test cases for C source files. It leverages the power of Large Language Models (LLMs) to analyze C functions and intelligently create comprehensive test suites, significantly reducing the manual effort required for unit testing in kernel development and other C projects.

The tool parses C code to extract function definitions, arguments, and return types, then constructs a detailed prompt for an LLM to generate the corresponding KUnit test code.

Key Features:
Automated Test Generation: Automatically creates KUnit test cases for specified C functions.
C Code Parsing: Accurately parses C source files to identify function signatures and structures.
LLM Integration: Connects to a Large Language Model to generate context-aware and syntactically correct test code.
Command-Line Interface: Easy-to-use CLI for specifying source files, function names, and output locations.
Reduces Manual Effort: Drastically cuts down the time and effort required to write thorough unit tests.
Customizable: Can be adapted to work with different LLMs and project structures.

How It Works:

Parsing: The tool uses a C parser to analyze the input C file and build an Abstract Syntax Tree (AST).
Function Extraction: It traverses the AST to find the specific function you want to test and extracts its complete signature (name, return type, and parameters).
Prompt Engineering: It constructs a detailed, well-structured prompt containing the function's code and signature. This prompt instructs the LLM to generate a KUnit test case.
LLM Inference: The prompt is sent to a configured LLM endpoint.
Code Generation: The LLM processes the prompt and returns a complete KUnit test suite for the function.
Output: The generated test code is saved to a specified output file, ready to be compiled and run.

Getting Started

Prerequisites

Python 3.8+
C compiler (like GCC,aocc,kernel..etc) for parsing capabilities
Access to a Large Language Model API (the tool is designed to be model-agnostic, but you will need to plug in your own API call in llm.py).

Installation

Clone the repository:

git clone https://github.com/nithin504/Kunit_TestGen.git
cd Kunit_TestGen

Install the required Python packages:

# It is recommended to use a virtual environment
python -m venv venv
source venv/bin/activate  # On Windows use `venv\Scripts\activate`
pip install -r requirements.txt

Configure the API:
Create .env file add API Key.

Usage
Run the exreact_funcation.py to extract function from requried "C" file then run generate_Kunit_test.py to generate the Kunit test cases.

