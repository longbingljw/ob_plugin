import java.io.StringReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.custom.CustomAnalyzer;
import org.apache.lucene.analysis.tokenattributes.CharTermAttribute;

/**
 * Korean Segmenter using CustomAnalyzer (ES Database Best Practice)
 * Equivalent to ES config: nori_tokenizer + lowercase (no excessive filtering)
 */
public class KoreanSegmenter {
    private Analyzer analyzer;
    private boolean initialized = false;

    /**
     * Constructor
     */
    public KoreanSegmenter() {
        try {
            // 使用CustomAnalyzer实现ES数据库场景最佳实践
            this.analyzer = CustomAnalyzer.builder()
                .withTokenizer("korean")        // nori_tokenizer (韩语核心分词)
                .addTokenFilter("lowercase")    // lowercase (基础标准化)
                // 关键：不添加过度的词性过滤器
                // 不添加: koreanPartOfSpeechStop (避免过度过滤)
                // 不添加: koreanReadingForm (避免词形转换)
                .build();
                
            this.initialized = true;
            System.out.println("KoreanSegmenter initialized with ES Database Best Practice (CustomAnalyzer)");
            System.out.println("Filters: korean + lowercase (no excessive filtering)");
            System.out.println("Excluded: koreanPartOfSpeechStop, koreanReadingForm (to preserve grammar info)");
        } catch (Exception e) {
            System.err.println("Failed to initialize CustomAnalyzer for Korean: " + e.getMessage());
            e.printStackTrace();
            this.initialized = false;
        }
    }

    /**
     * Segment Korean text into tokens
     * @param text The Korean text to segment
     * @return Array of token strings
     */
    public String[] segment(String text) {
        if (!initialized) {
            System.err.println("KoreanSegmenter is not properly initialized");
            return new String[0];
        }

        if (text == null || text.trim().isEmpty()) {
            System.out.println("=== OCEANBASE JNI CALL === Korean segmentation: empty input");
            return new String[0];
        }

        System.out.println("=== OCEANBASE JNI CALL === Segmenting Korean text with ES CustomAnalyzer: \"" + text + "\" (length: " + text.length() + ")");

        List<String> tokens = new ArrayList<>();

        try {
            TokenStream tokenStream = analyzer.tokenStream("content", new StringReader(text));
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

        String[] result = tokens.toArray(new String[0]);
        System.out.println("=== OCEANBASE JNI RESULT === ES CustomAnalyzer Korean result: " + java.util.Arrays.toString(result));

        // Debug: Print each token's byte and char length
        for (int i = 0; i < result.length; i++) {
            try {
                byte[] utf8Bytes = result[i].getBytes("UTF-8");
                System.out.println("  Token[" + i + "]: '" + result[i] + "' -> " + utf8Bytes.length + " bytes, " + result[i].length() + " chars");
            } catch (Exception e) {
                System.out.println("  Token[" + i + "]: '" + result[i] + "' -> encoding error: " + e.getMessage());
            }
        }

        return result;
    }

    /**
     * Cleanup resources
     */
    public void close() {
        if (analyzer != null) {
            analyzer.close();
            System.out.println("KoreanSegmenter closed");
        }
    }

    /**
     * Main method for testing
     */
    public static void main(String[] args) {
        KoreanSegmenter segmenter = new KoreanSegmenter();

        // Test cases
        String[] testTexts = {
            "안녕하세요",
            "데이터베이스",
            "학교에 갑니다",
            "한국어 처리",
            "OpenBase 데이터베이스 관리 시스템"
        };

        for (String text : testTexts) {
            System.out.println("\nTesting: \"" + text + "\"");
            String[] tokens = segmenter.segment(text);
            System.out.println("Tokens: " + java.util.Arrays.toString(tokens));
        }

        segmenter.close();
    }
}
