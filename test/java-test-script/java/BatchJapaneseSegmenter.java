import java.io.*;
import java.nio.file.*;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Arrays;

/**
 * Batch Japanese Segmenter for processing text files
 * Reads input file line by line, segments each line, and writes results to timestamped output file
 */
public class BatchJapaneseSegmenter {
    private JapaneseSegmenter segmenter;
    private final String RESULTS_DIR = "jp_results";

    public BatchJapaneseSegmenter() {
        this.segmenter = new JapaneseSegmenter();
        createResultsDirectory();
    }

    /**
     * Create results directory if it doesn't exist
     */
    private void createResultsDirectory() {
        try {
            Files.createDirectories(Paths.get(RESULTS_DIR));
            System.out.println("Results directory created: " + RESULTS_DIR);
        } catch (IOException e) {
            System.err.println("Failed to create results directory: " + e.getMessage());
        }
    }

    /**
     * Generate timestamped filename for Japanese segmentation results
     * Format: jp_YYYYMMDD_HHMMSS.txt
     */
    private String generateTimestampedFilename() {
        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyyMMdd_HHmmss");
        String timestamp = LocalDateTime.now().format(formatter);
        return "jp_" + timestamp + ".txt";
    }

    /**
     * Process input file: read line by line, segment each line, write to output file
     * @param inputFilePath Path to the input text file
     * @return Path to the generated output file, or null if processing failed
     */
    public String processFile(String inputFilePath) {
        Path inputPath = Paths.get(inputFilePath);

        // Check if input file exists
        if (!Files.exists(inputPath)) {
            System.err.println("Input file does not exist: " + inputFilePath);
            return null;
        }

        // Generate output filename
        String outputFilename = generateTimestampedFilename();
        Path outputPath = Paths.get(RESULTS_DIR, outputFilename);

        System.out.println("Processing file: " + inputFilePath);
        System.out.println("Output will be saved to: " + outputPath.toString());

        int processedLines = 0;
        int totalTokens = 0;

        try (BufferedReader reader = Files.newBufferedReader(inputPath);
             BufferedWriter writer = Files.newBufferedWriter(outputPath)) {

            String line;
            while ((line = reader.readLine()) != null) {
                processedLines++;

                // Skip empty lines
                if (line.trim().isEmpty()) {
                    writer.write("\n");
                    continue;
                }

                // Segment the line
                String[] tokens = segmenter.segment(line.trim());

                // Check if segmentation produced tokens
                if (tokens != null && tokens.length > 0) {
                    // Join tokens with commas
                    String result = String.join(",", tokens);

                    // Write result followed by newline
                    writer.write(result);
                    writer.newLine();

                    totalTokens += tokens.length;
                } else {
                    // If no tokens produced, write the original text
                    // This handles cases where the text cannot be segmented (e.g., Korean text with Japanese segmenter)
                    writer.write(line.trim());
                    writer.newLine();

                    System.out.println("Warning: Line " + processedLines + " produced no tokens, writing original text: '" + line.trim() + "'");
                }

                // Progress indicator for large files
                if (processedLines % 100 == 0) {
                    System.out.println("Processed " + processedLines + " lines...");
                }
            }

            System.out.println("Processing completed:");
            System.out.println("  - Total lines processed: " + processedLines);
            System.out.println("  - Total tokens generated: " + totalTokens);
            System.out.println("  - Output file: " + outputPath.toString());

            return outputPath.toString();

        } catch (IOException e) {
            System.err.println("Error processing file: " + e.getMessage());
            return null;
        }
    }

    /**
     * Cleanup resources
     */
    public void cleanup() {
        if (segmenter != null) {
            segmenter.cleanup();
        }
    }

    /**
     * Main method for command line usage
     * Usage: java BatchJapaneseSegmenter <input_file.txt>
     */
    public static void main(String[] args) {
        if (args.length != 1) {
            System.out.println("Usage: java BatchJapaneseSegmenter <input_file.txt>");
            System.out.println("Example: java BatchJapaneseSegmenter japanese_text.txt");
            return;
        }

        String inputFile = args[0];
        BatchJapaneseSegmenter batchProcessor = new BatchJapaneseSegmenter();

        try {
            String outputFile = batchProcessor.processFile(inputFile);
            if (outputFile != null) {
                System.out.println("✅ Batch processing completed successfully!");
                System.out.println("📁 Output file: " + outputFile);
            } else {
                System.out.println("❌ Batch processing failed!");
                System.exit(1);
            }
        } finally {
            batchProcessor.cleanup();
        }
    }
}
