import java.io.StringReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.custom.CustomAnalyzer;
import org.apache.lucene.analysis.tokenattributes.CharTermAttribute;

/**
 * Japanese Segmenter using static method to reduce memory usage
 */
public class JapaneseSegmenter {
    private static volatile Analyzer staticAnalyzer;
    private static final Object initLock = new Object();
    private static boolean staticInitialized = false;
    
    static {
        initializeStaticAnalyzer();
    }
    
    private static void initializeStaticAnalyzer() {
        try {
            staticAnalyzer = CustomAnalyzer.builder()
                .withTokenizer("japanese")                    // kuromoji_tokenizer
                .addTokenFilter("japaneseBaseForm")           // kuromoji_baseform
                .addTokenFilter("japanesePartOfSpeechStop")   // kuromoji_part_of_speech
                .addTokenFilter("cjkWidth")                   // cjk_width
                .addTokenFilter("lowercase")                  // lowercase
                .addTokenFilter("stop", "words", "org/apache/lucene/analysis/ja/stopwords.txt")  // ja_stop
                .build();
                
            staticInitialized = true;
            System.out.println("JapaneseSegmenter initialized");
        } catch (Exception e) {
            System.err.println("Failed to initialize JapaneseSegmenter: " + e.getMessage());
            e.printStackTrace();
            staticInitialized = false;
        }
    }
    
    /**
     * Segment Japanese text into tokens
     * @param text The input text to segment
     * @return Array of segmented tokens
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
            throw new IllegalStateException("JapaneseSegmenter not initialized");
        }
        
        if (text == null || text.trim().isEmpty()) {
            return new String[0];
        }
        
        List<String> tokens = new ArrayList<>();
        
        try {
            TokenStream tokenStream = staticAnalyzer.tokenStream("content", new StringReader(text));
            CharTermAttribute termAttr = tokenStream.addAttribute(CharTermAttribute.class);
            
            tokenStream.reset();
            while (tokenStream.incrementToken()) {
                String token = termAttr.toString().trim();
                if (!token.isEmpty()) {
                    tokens.add(token);
                }
            }
            tokenStream.end();
            tokenStream.close();
            
        } catch (IOException e) {
            System.err.println("Error during tokenization: " + e.getMessage());
            return new String[0];
        }
        
        return tokens.toArray(new String[0]);
    }
    
    public static boolean isStaticInitialized() {
        return staticInitialized;
    }
    
    public static void main(String[] args) {
        // Test cases
        String[] result1 = JapaneseSegmenter.segment("Hello world");
        System.out.println("English: " + java.util.Arrays.toString(result1));
        
        String[] result2 = JapaneseSegmenter.segment("私は学生です");
        System.out.println("Japanese: " + java.util.Arrays.toString(result2));
        
        String[] result3 = JapaneseSegmenter.segment("Hello こんにちは world");
        System.out.println("Mixed: " + java.util.Arrays.toString(result3));
        
        String[] result4 = JapaneseSegmenter.segment("東京都渋谷区でコンピューターを勉強しています");
        System.out.println("Complex: " + java.util.Arrays.toString(result4));
        
        String[] result5 = JapaneseSegmenter.segment("OceanBaseデータベースを選ぶ理由");
        System.out.println("OceanBase: " + java.util.Arrays.toString(result5));
    }
}