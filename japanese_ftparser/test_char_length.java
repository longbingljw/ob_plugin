import java.io.UnsupportedEncodingException;

public class test_char_length {
    public static void main(String[] args) throws UnsupportedEncodingException {
        String[] words = {"理由", "選ぶ", "データベース", "oceanbase", "こんにちは"};
        
        for (String word : words) {
            byte[] utf8Bytes = word.getBytes("UTF-8");
            System.out.println("'" + word + "' -> " + utf8Bytes.length + " bytes, " + word.length() + " chars");
            
            // Print hex representation
            System.out.print("  Hex: ");
            for (byte b : utf8Bytes) {
                System.out.printf("%02X ", b);
            }
            System.out.println();
        }
    }
}
