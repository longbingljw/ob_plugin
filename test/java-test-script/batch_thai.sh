#!/bin/bash

# Batch Thai Segmenter Script
# Processes a text file line by line and outputs segmented results to timestamped file

echo "🌍 Batch Thai Text Segmenter"
echo "Language: Thai 🇹🇭"
echo ""

# Check if input file is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <input_file.txt>"
    echo "Example: $0 thai_text.txt"
    echo ""
    echo "This script will:"
    echo "  1. Read the input file line by line"
    echo "  2. Segment each line using Thai analyzer (ThaiAnalyzer)"
    echo "  3. Join tokens with commas"
    echo "  4. Save results to th_results/th_YYYYMMDD_HHMMSS.txt"
    exit 1
fi

INPUT_FILE="$1"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "❌ Error: Input file '$INPUT_FILE' does not exist!"
    exit 1
fi

echo "📁 Input file: $INPUT_FILE"
echo "🔄 Processing with Thai segmenter..."
echo ""

# Change to java directory and run the batch processor
cd java
# Convert relative path to absolute path if needed
if [[ "$INPUT_FILE" = /* ]]; then
    ABS_INPUT_FILE="$INPUT_FILE"
else
    ABS_INPUT_FILE="../$INPUT_FILE"
fi
java -cp ".:lib/*" BatchThaiSegmenter "$ABS_INPUT_FILE"
