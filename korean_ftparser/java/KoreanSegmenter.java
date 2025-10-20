import java.io.StringReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.custom.CustomAnalyzer;
import org.apache.lucene.analysis.tokenattributes.CharTermAttribute;

/**
 * Korean Segmenter using static method to reduce memory usage
 */
public class KoreanSegmenter {
    private static volatile Analyzer staticAnalyzer;
    private static final Object initLock = new Object();
    private static boolean staticInitialized = false;
    
    static {
        initializeStaticAnalyzer();
    }
    
    private static void initializeStaticAnalyzer() {
        try {
            staticAnalyzer = CustomAnalyzer.builder()
                .withTokenizer("korean", "decompoundMode", "mixed")  // MIXED mode!
                .addTokenFilter("lowercase")    // lowercase (basic normalization)
                .build();
                
            staticInitialized = true;
            System.out.println("KoreanSegmenter initialized with MIXED mode");
        } catch (Exception e) {
            System.err.println("Failed to initialize KoreanSegmenter: " + e.getMessage());
            e.printStackTrace();
            staticInitialized = false;
        }
    }
    
    /**
     * Segment Korean text into tokens
     * @param text The Korean text to segment
     * @return Array of token strings
     */
    public static String[] segment(String text) {
        if (!staticInitialized) {
            synchronized (initLock) {
                if (!staticInitialized) {
                    initializeStaticAnalyzer();
                }
            }
        }
        
        if (!staticInitialized) {
            throw new IllegalStateException("KoreanSegmenter not initialized");
        }

        if (text == null || text.trim().isEmpty()) {
            return new String[0];
        }

        List<String> tokens = new ArrayList<>();

        try {
            TokenStream tokenStream = staticAnalyzer.tokenStream("content", new StringReader(text));
            CharTermAttribute attr = tokenStream.addAttribute(CharTermAttribute.class);
            
            tokenStream.reset();
            while (tokenStream.incrementToken()) {
                String token = attr.toString();
                if (token != null && !token.trim().isEmpty()) {
                    tokens.add(token);
                }
            }
            tokenStream.end();
            tokenStream.close();

        } catch (IOException e) {
            System.err.println("Error during Korean tokenization: " + e.getMessage());
            return new String[0];
        }

        return tokens.toArray(new String[0]);
    }
    
    public static boolean isStaticInitialized() {
        return staticInitialized;
    }
    
    public static void main(String[] args) {
        // Test cases
        String[] testTexts = {
            "안녕하세요",
            "데이터베이스",
            "학교에 갑니다",
            "한국어 처리",
            "OceanBase 데이터베이스 관리 시스템"
        };

        for (String text : testTexts) {
            System.out.println("Testing: \"" + text + "\"");
            String[] tokens = KoreanSegmenter.segment(text);
            System.out.println("Tokens: " + java.util.Arrays.toString(tokens));
        }
    }
}