import java.io.StringReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.th.ThaiAnalyzer;
import org.apache.lucene.analysis.tokenattributes.CharTermAttribute;

/**
 * Thai Segmenter using static method to reduce memory usage
 */
public class ThaiSegmenter {
    private static volatile ThaiAnalyzer staticAnalyzer;
    private static final Object initLock = new Object();
    private static boolean staticInitialized = false;
    
    static {
        initializeStaticAnalyzer();
    }
    
    private static void initializeStaticAnalyzer() {
        try {
            staticAnalyzer = new ThaiAnalyzer();
            staticInitialized = true;
            System.out.println("ThaiSegmenter initialized");
        } catch (Exception e) {
            System.err.println("Failed to initialize ThaiSegmenter: " + e.getMessage());
            e.printStackTrace();
            staticInitialized = false;
        }
    }
    
    /**
     * Segment Thai text into tokens
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
            throw new IllegalStateException("ThaiSegmenter not initialized");
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
            System.err.println("Error during Thai tokenization: " + e.getMessage());
            return new String[0];
        }
        
        return tokens.toArray(new String[0]);
    }
    
    public static boolean isStaticInitialized() {
        return staticInitialized;
    }
    
    public static void main(String[] args) {
        // Test cases
        String[] result1 = ThaiSegmenter.segment("Hello world");
        System.out.println("English: " + java.util.Arrays.toString(result1));
        
        String[] result2 = ThaiSegmenter.segment("สวัสดีครับ");
        System.out.println("Thai: " + java.util.Arrays.toString(result2));
        
        String[] result3 = ThaiSegmenter.segment("Hello สวัสดี world");
        System.out.println("Mixed: " + java.util.Arrays.toString(result3));
        
        String[] result4 = ThaiSegmenter.segment("ฐานข้อมูล OceanBase เป็นระบบจัดการฐานข้อมูล");
        System.out.println("Complex: " + java.util.Arrays.toString(result4));
    }
}