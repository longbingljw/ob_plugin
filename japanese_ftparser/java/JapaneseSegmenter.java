import java.io.StringReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.Tokenizer;
import org.apache.lucene.analysis.ja.JapaneseTokenizer;
import org.apache.lucene.analysis.ja.JapanesePartOfSpeechStopFilter;
import org.apache.lucene.analysis.ja.JapaneseAnalyzer;
import org.apache.lucene.analysis.cjk.CJKWidthFilter;
import org.apache.lucene.analysis.core.LowerCaseFilter;
import org.apache.lucene.analysis.tokenattributes.CharTermAttribute;

/**
 * Japanese Segmenter using ES Database Best Practice
 * Equivalent to ES config: kuromoji_tokenizer + kuromoji_part_of_speech + cjk_width + lowercase
 */
public class JapaneseSegmenter {
    private Analyzer analyzer;
    private boolean initialized = false;
    
    /**
     * Constructor
     */
    public JapaneseSegmenter() {
        try {
            // 不创建Analyzer，直接在segment方法中处理
            this.analyzer = null;
            this.initialized = true;
            System.out.println("JapaneseSegmenter initialized with ES Database Best Practice (single .class)");
            System.out.println("Will apply: kuromoji_tokenizer + kuromoji_part_of_speech + cjk_width + lowercase");
            System.out.println("Excluded: kuromoji_baseform (to avoid stemming for database use)");
        } catch (Exception e) {
            System.err.println("Failed to initialize JapaneseSegmenter: " + e.getMessage());
            e.printStackTrace();
            this.initialized = false;
        }
    }
    
    /**
     * Segment Japanese text into tokens using ES Database Best Practice
     * @param text The input text to segment
     * @return Array of segmented tokens
     */
    public String[] segment(String text) {
        if (!initialized) {
            throw new IllegalStateException("JapaneseSegmenter not initialized");
        }
        
        if (text == null || text.trim().isEmpty()) {
            return new String[0];
        }
        
        System.out.println("=== OCEANBASE JNI CALL === Segmenting text with ES Database Style: \"" + text + "\" (length: " + text.length() + ")");
        
        List<String> tokens = new ArrayList<>();
        
        try {
            // 按ES数据库场景配置处理
            
            // 1. kuromoji_tokenizer: 使用Kuromoji核心分词
            JapaneseTokenizer tokenizer = new JapaneseTokenizer(null, false, JapaneseTokenizer.Mode.NORMAL);
            tokenizer.setReader(new StringReader(text));
            
            // 2. kuromoji_part_of_speech: 词性停用词过滤
            TokenStream stream1 = new JapanesePartOfSpeechStopFilter(tokenizer, JapaneseAnalyzer.getDefaultStopTags());
            
            // 3. cjk_width: CJK宽度标准化
            TokenStream stream2 = new CJKWidthFilter(stream1);
            
            // 4. lowercase: 大小写标准化
            TokenStream finalStream = new LowerCaseFilter(stream2);
            
            // ❌ 不包含: kuromoji_baseform (JapaneseBaseFormFilter)
            
            CharTermAttribute termAttr = finalStream.addAttribute(CharTermAttribute.class);
            
            finalStream.reset();
            while (finalStream.incrementToken()) {
                String token = termAttr.toString().trim();
                if (!token.isEmpty()) {
                    tokens.add(token);
                }
            }
            finalStream.end();
            finalStream.close();
            
        } catch (IOException e) {
            System.err.println("Error during tokenization: " + e.getMessage());
            e.printStackTrace();
            return new String[0];
        }
        
        String[] result = tokens.toArray(new String[0]);
        System.out.println("=== OCEANBASE JNI RESULT === ES Database style result: " + java.util.Arrays.toString(result));
        return result;
    }
    
    /**
     * Cleanup resources
     */
    public void cleanup() {
        System.out.println("JapaneseSegmenter cleanup called");
        if (analyzer != null) {
            analyzer.close();
        }
        initialized = false;
    }
    
    /**
     * Test main method
     */
    public static void main(String[] args) {
        JapaneseSegmenter segmenter = new JapaneseSegmenter();
        
        // Test with English text
        String[] result1 = segmenter.segment("Hello world");
        System.out.println("English test: " + java.util.Arrays.toString(result1));
        
        // Test with Japanese text
        String[] result2 = segmenter.segment("私は学生です");
        System.out.println("Japanese test: " + java.util.Arrays.toString(result2));
        
        // Test with mixed text (English + Japanese)
        String[] result3 = segmenter.segment("Hello こんにちは world");
        System.out.println("Mixed test: " + java.util.Arrays.toString(result3));
        
        // Test with complex Japanese text
        String[] result4 = segmenter.segment("東京都渋谷区でコンピューターを勉強しています");
        System.out.println("Complex Japanese test: " + java.util.Arrays.toString(result4));
        
        // Test with mixed English-Japanese text (OceanBase example)
        String[] result5 = segmenter.segment("OceanBaseデータベースを選ぶ理由");
        System.out.println("OceanBase mixed test: " + java.util.Arrays.toString(result5));
        
        segmenter.cleanup();
    }
}