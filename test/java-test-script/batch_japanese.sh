#!/bin/bash

# Batch Japanese Segmenter Script
# Processes a text file line by line and outputs segmented results to timestamped file

echo "🌍 Batch Japanese Text Segmenter"
echo "Language: Japanese 🇯🇵"
echo ""

# Check if input file is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <input_file.txt>"
    echo "Example: $0 japanese_text.txt"
    echo ""
    echo "This script will:"
    echo "  1. Read the input file line by line"
    echo "  2. Segment each line using Japanese analyzer (Kuromoji)"
    echo "  3. Join tokens with commas"
    echo "  4. Save results to jp_results/jp_YYYYMMDD_HHMMSS.txt"
    exit 1
fi

INPUT_FILE="$1"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "❌ Error: Input file '$INPUT_FILE' does not exist!"
    exit 1
fi

echo "📁 Input file: $INPUT_FILE"
echo "🔄 Processing with Japanese segmenter..."
echo ""

# Change to java directory and run the batch processor
cd java
# Convert relative path to absolute path if needed
if [[ "$INPUT_FILE" = /* ]]; then
    ABS_INPUT_FILE="$INPUT_FILE"
else
    ABS_INPUT_FILE="../$INPUT_FILE"
fi
java -cp ".:lib/*" BatchJapaneseSegmenter "$ABS_INPUT_FILE"
