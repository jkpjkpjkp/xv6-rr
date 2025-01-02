#!/usr/bin/env bash

OUTPUT="combined_kernel_sources.md"

# If an old output file exists, clear its contents
> "$OUTPUT"

# Loop through every file in the kernel/ directory
for file in kernel/*; do
    # Optionally add a heading for the file name
    echo "## ${file}" >> "$OUTPUT"
    
    # Start code fence
    echo '```' >> "$OUTPUT"
    
    # Concatenate the file contents
    cat "$file" >> "$OUTPUT"
    
    # End code fence
    echo '```' >> "$OUTPUT"
    echo >> "$OUTPUT"  # Extra blank line for spacing
done

echo "All kernel files have been concatenated into $OUTPUT."
