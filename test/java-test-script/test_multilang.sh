#!/bin/bash

# Multi-Language Segmenter Test Script
# Supports Japanese (Kuromoji), Korean (Nori), and Thai (ThaiAnalyzer)

echo "🌍 Starting Multi-Language Segmenter Test..."
echo "Languages: Japanese 🇯🇵 | Korean 🇰🇷 | Thai 🇹🇭"
echo ""

cd java
java -cp ".:lib/*" MultiLanguageSegmenterTest
