import os
from pathlib import Path

def clean_files_in_folder(folder_path: str, extensions=(".txt", ".c", ".py")):
    """
    Removes '''c in the first line and ''' in the last line
    from all files in the given folder (non-recursive).
    Saves cleaned content back to the same file.
    """
    folder = Path(folder_path)
    for file in folder.iterdir():  # non-recursive: only current folder
        if file.is_file() and file.suffix in extensions:
            with file.open("r", encoding="utf-8") as f:
                lines = f.readlines()

            if not lines:
                continue  # skip empty files

            # Remove first line if it is '''c
            if lines[0].strip() == "'''c":
                lines = lines[1:]

            # Remove last line if it is '''
            if lines and lines[-1].strip() == "'''":
                lines = lines[:-1]

            # Write back cleaned content
            with file.open("w", encoding="utf-8") as f:
                f.writelines(lines)

            print(f"Cleaned: {file.name}")


if __name__ == "__main__":
    folder_to_clean = "./functions_extract"  # change this to your folder
    clean_files_in_folder(folder_to_clean, extensions=(".c", ".txt", ".py"))
