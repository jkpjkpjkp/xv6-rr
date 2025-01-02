#!/usr/bin/env python3

import os
import sys

# Customize the extension-to-language map as needed
EXTENSION_MAP = {
    '.c':   'c',
    '.h':   'c',
    '.cc':  'cpp',
    '.cpp': 'cpp',
    '.hpp': 'cpp',
    '.py':  'python',
    '.sh':  'bash',
    '.js':  'javascript',
    '.ts':  'typescript',
    '.go':  'go',
    '.rb':  'ruby',
    '.java':'java',
    '.kt':  'kotlin'
}

OUTPUT_FILE = "combined_kernel_sources.md"

def main():
    # If an old output file exists, clear its contents
    open(OUTPUT_FILE, 'w').close()

    # Loop through files in kernel/ 
    for filename in sorted(os.listdir("kernel")):
        # Construct the full path
        full_path = os.path.join("kernel", filename)
        
        # Skip subdirectories, just in case
        if not os.path.isfile(full_path):
            continue

        # Get file extension to decide on language hint
        _, extension = os.path.splitext(filename)
        language_hint = EXTENSION_MAP.get(extension, "")  # Fallback to empty if unknown

        # Write file name and contents to the combined file
        with open(OUTPUT_FILE, 'a', encoding='utf-8') as out_f:
            out_f.write(f"## {filename}\n\n")
            out_f.write(f"```{language_hint}\n")
            with open(full_path, 'r', encoding='utf-8') as in_f:
                out_f.write(in_f.read())
            out_f.write("\n```\n\n")

    print(f"All kernel files have been concatenated into {OUTPUT_FILE}.")

if __name__ == "__main__":
    main()

