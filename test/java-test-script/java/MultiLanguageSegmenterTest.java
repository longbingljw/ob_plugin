import java.util.Scanner;

/**
 * Multi-Language Segmenter Interactive Test
 * Supports Japanese, Korean, and Thai text segmentation
 */
public class MultiLanguageSegmenterTest {
    
    private JapaneseSegmenter japaneseSegmenter;
    private KoreanSegmenter koreanSegmenter;
    private ThaiSegmenter thaiSegmenter;  // Changed from RealThaiSegmenter
    
    public MultiLanguageSegmenterTest() {
        System.out.println("=== Initializing Multi-Language Segmenters ===");
        
        try {
            japaneseSegmenter = new JapaneseSegmenter();
            System.out.println("✓ Japanese segmenter initialized");
        } catch (Exception e) {
            System.err.println("✗ Failed to initialize Japanese segmenter: " + e.getMessage());
        }
        
        try {
            koreanSegmenter = new KoreanSegmenter();
            System.out.println("✓ Korean segmenter initialized");
        } catch (Exception e) {
            System.err.println("✗ Failed to initialize Korean segmenter: " + e.getMessage());
        }
        
        try {
            thaiSegmenter = new ThaiSegmenter();  // Changed from RealThaiSegmenter
            System.out.println("✓ Thai segmenter initialized");
        } catch (Exception e) {
            System.err.println("✗ Failed to initialize Thai segmenter: " + e.getMessage());
        }
        
        System.out.println();
    }
    
    public static void main(String[] args) {
        MultiLanguageSegmenterTest tester = new MultiLanguageSegmenterTest();
        Scanner scanner = new Scanner(System.in);
        
        System.out.println("=== Multi-Language Segmenter Interactive Test ===");
        System.out.println("Select language mode:");
        System.out.println("  1. Japanese (日本語) 🇯🇵");
        System.out.println("  2. Korean (한국어) 🇰🇷");
        System.out.println("  3. Thai (ภาษาไทย) 🇹🇭");
        System.out.println("  help - Show examples");
        System.out.println("  quit - Exit");
        System.out.println();
        
        String currentMode = null;
        String modePrompt = "Select mode (1-3): ";
        
        while (true) {
            System.out.print(modePrompt);
            String input = scanner.nextLine().trim();
            
            if (input.isEmpty()) {
                continue;
            }
            
            if ("quit".equalsIgnoreCase(input) || "stop".equalsIgnoreCase(input)) {
                System.out.println("Exiting...");
                break;
            }
            
            if ("help".equalsIgnoreCase(input)) {
                tester.showExamples();
                continue;
            }
            
            // Mode selection
            if (currentMode == null) {
                if ("1".equals(input) || "jp".equalsIgnoreCase(input) || "japanese".equalsIgnoreCase(input)) {
                    currentMode = "jp";
                    modePrompt = "JP_Input: ";
                    System.out.println("🇯🇵 Japanese mode activated. Enter text to segment (type 'switch' to change language):");
                } else if ("2".equals(input) || "ko".equalsIgnoreCase(input) || "korean".equalsIgnoreCase(input)) {
                    currentMode = "ko";
                    modePrompt = "KO_Input: ";
                    System.out.println("🇰🇷 Korean mode activated. Enter text to segment (type 'switch' to change language):");
                } else if ("3".equals(input) || "th".equalsIgnoreCase(input) || "thai".equalsIgnoreCase(input)) {
                    currentMode = "th";
                    modePrompt = "TH_Input: ";
                    System.out.println("🇹🇭 Thai mode activated. Enter text to segment (type 'switch' to change language):");
                } else {
                    System.out.println("Invalid selection. Please enter 1, 2, or 3.");
                }
                continue;
            }
            
            // Text processing in current mode
            if ("switch".equalsIgnoreCase(input)) {
                System.out.println("\n🔄 Switch to which language?");
                System.out.println("  1. Japanese 🇯🇵  2. Korean 🇰🇷  3. Thai 🇹🇭");
                System.out.print("Select (1-3): ");
                String switchChoice = scanner.nextLine().trim();
                
                if ("1".equals(switchChoice)) {
                    currentMode = "jp";
                    modePrompt = "JP_Input: ";
                    System.out.println("🇯🇵 Switched to Japanese mode:");
                } else if ("2".equals(switchChoice)) {
                    currentMode = "ko";
                    modePrompt = "KO_Input: ";
                    System.out.println("🇰🇷 Switched to Korean mode:");
                } else if ("3".equals(switchChoice)) {
                    currentMode = "th";
                    modePrompt = "TH_Input: ";
                    System.out.println("🇹🇭 Switched to Thai mode:");
                } else {
                    System.out.println("Invalid choice, staying in current mode.");
                }
                continue;
            }
            
            // Process text with current segmenter
            tester.processText(currentMode, input);
            System.out.println();
        }
        
        tester.cleanup();
        scanner.close();
        System.out.println("Test completed.");
    }
    
    private void processText(String mode, String text) {
        switch (mode) {
            case "jp":
                testJapanese(text);
                break;
            case "ko":
                testKorean(text);
                break;
            case "th":
                testThai(text);
                break;
            default:
                System.out.println("Unknown mode: " + mode);
        }
    }
    
    private void testJapanese(String text) {
        if (japaneseSegmenter != null) {
            try {
                String[] result = japaneseSegmenter.segment(text);
                System.out.println("Result: " + java.util.Arrays.toString(result));
                System.out.println("Token count: " + result.length);
            } catch (Exception e) {
                System.err.println("Error: " + e.getMessage());
            }
        } else {
            System.err.println("Japanese segmenter not available");
        }
    }
    
    private void testKorean(String text) {
        if (koreanSegmenter != null) {
            try {
                String[] result = koreanSegmenter.segment(text);
                System.out.println("Result: " + java.util.Arrays.toString(result));
                System.out.println("Token count: " + result.length);
            } catch (Exception e) {
                System.err.println("Error: " + e.getMessage());
            }
        } else {
            System.err.println("Korean segmenter not available");
        }
    }
    
    private void testThai(String text) {
        if (thaiSegmenter != null) {
            try {
                String[] result = thaiSegmenter.segment(text);
                System.out.println("Result: " + java.util.Arrays.toString(result));
                System.out.println("Token count: " + result.length);
            } catch (Exception e) {
                System.err.println("Error: " + e.getMessage());
            }
        } else {
            System.err.println("Thai segmenter not available");
        }
    }
    
    
    private void showHelp() {
        System.out.println("\n=== Usage Instructions ===");
        System.out.println("1. Select language mode (1-3)");
        System.out.println("2. Enter text to segment");
        System.out.println("3. Type 'switch' to change language");
        System.out.println("4. Type 'quit' to exit");
        System.out.println("\nSupported languages:");
        System.out.println("  🇯🇵 Japanese (Kuromoji)");
        System.out.println("  🇰🇷 Korean (Nori)");
        System.out.println("  🇹🇭 Thai (ThaiAnalyzer)");
        System.out.println();
    }
    
    private void showExamples() {
        System.out.println("\n=== Example Texts ===");
        System.out.println("🇯🇵 Japanese examples:");
        System.out.println("  私は学生です");
        System.out.println("  東京都渋谷区でコンピューターを勉強しています");
        System.out.println("  OceanBaseデータベースを選ぶ理由");
        
        System.out.println("\n🇰🇷 Korean examples:");
        System.out.println("  안녕하세요");
        System.out.println("  데이터베이스");
        System.out.println("  OpenBase 데이터베이스 관리 시스템");
        
        System.out.println("\n🇹🇭 Thai examples:");
        System.out.println("  สวัสดีครับ");
        System.out.println("  test");
        System.out.println("  Hello สวัสดี world");
        
        System.out.println();
    }
    
    private void cleanup() {
        System.out.println("Cleaning up resources...");
        if (japaneseSegmenter != null) {
            japaneseSegmenter.cleanup();
        }
        if (koreanSegmenter != null) {
            // Korean segmenter doesn't have close() method, it's cleanup-free
            System.out.println("Korean segmenter closed");
        }
        if (thaiSegmenter != null) {
            thaiSegmenter.cleanup();
        }
        System.out.println("Cleanup completed.");
    }
}