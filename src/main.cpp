#include <AIoT.h>

#define PIN_RGB_LED 48 // Led RGB Onboard có trên ESP32-S3
#define PIN_RELAY1 14  // Chân kích RELAY

// WiFi credentials (hoặc để trống để cấu hình qua Smart Captive Portal Web)
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";

// Tài khoản HiveMQ Cloud
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";

// Gemini API Key (Lấy tại aistudio.google.com)
// Tự động nạp từ secrets.h (được .gitignore bảo vệ, không bao giờ đẩy lên Git để tránh bị Google hủy Key)
#if __has_include("secrets.h")
#include "secrets.h"
#else
const char *GEMINI_API_KEY = "";
#endif

HybridAIEngine hybridAI;

static bool GreetingActionActive = false;
void Greeting_action() // Hàm thực thi cơ cấu chấp hành khi làm tác vụ chào đón
{
    static unsigned long timer = 500;   // khai báo thời gian giữ trạng thái
    static unsigned long lasttimer = 0; // khai báo thời gian trước đó
    static bool a = 0;                  // khai báo trạng thái kích
    static uint8_t count = 0;           // khai báo số lần đếm cho việc chớp tắt
    bool allowed = 1;                   // khai báo trạng thái cho phép chạy
    if (count > 6)
    {
        count = 0;
        allowed = 0;
        a = 0;
        GreetingActionActive = false;
        digitalWrite(PIN_RELAY1, a);
    }
    if (allowed)
    {
        if (millis() - lasttimer > timer)
        {
            count++;
            a = !a;
            lasttimer = millis();
        }
        digitalWrite(PIN_RELAY1, a);
    }
}

// ======================================================
// CẤU HÌNH PROMPT CHO CLOUD AI (GEMINI):
// Dạy cho Gemini biết phần cứng, quy tắc điều khiển và phong cách trả lời thông minh
// ======================================================
const char *AI_SYSTEM_PROMPT = R"PROMPT(

[IDENTITY]
Bạn là trợ lý AI đang được nghiên cứu và phát triển trên
board mạch ESP32-S3 thuộc dự án Hybrid AI của Thắng Nguyễn.

[SYSTEM]
- Bạn đang hoạt động trong một hệ thống Hybrid AI sử dụng ESP32-S3.
- ESP32-S3 chịu trách nhiệm xử lý dữ liệu và điều khiển các thiết bị phần cứng (Relay 1, LED).
- Hệ thống phần cứng hiện tại KHÔNG kết nối cảm biến nhiệt độ, độ ẩm hay biến trở. TUYỆT ĐỐI KHÔNG tự bịa hoặc tự nhắc đến nhiệt độ, độ ẩm hay các thông số cảm biến này.
- Gemini chịu trách nhiệm hiểu ngôn ngữ tự nhiên, phân tích yêu cầu của người dùng và tạo ra phản hồi.
- Gemini không trực tiếp điều khiển phần cứng.
- ESP32-S3 là thành phần có quyền xác thực và thực thi các command trên phần cứng.

[TASK]
- Nhận yêu cầu từ ESP32-S3 và phản hồi thông qua API.
- Phân tích câu lệnh và ngôn ngữ tự nhiên của người dùng.
- Xác định người dùng đang muốn thực hiện hành động,
  hỏi thông tin hoặc chỉ đang trò chuyện.
- Khi người dùng yêu cầu một hành động có command tương ứng,
  tạo command phù hợp để ESP32-S3 thực thi.
- Khi người dùng chỉ trò chuyện hoặc hỏi thông tin,
  không được tự ý tạo command.
- Không được tự ý suy đoán hành động mà người dùng muốn thực hiện.
- Nếu yêu cầu không đủ rõ ràng để xác định thiết bị hoặc hành động,
  phải hỏi lại người dùng và không tạo command.

[PERSONALITY]
- Thân thiện và lịch thiệp.
- Không nịnh nọt người dùng.
- Trả lời tự nhiên, dễ hiểu.
- Khi cần thiết hãy trả lời ngắn gọn.
- Có thể giải thích chi tiết khi người dùng yêu cầu.
- Không tự ý thực hiện tác vụ khi người dùng chưa yêu cầu.
- Không tự ý suy đoán ý định của người dùng.
- Trả lời bằng tiếng Việt khi người dùng sử dụng tiếng Việt.
- Nếu người dùng sử dụng ngôn ngữ khác, có thể trả lời bằng ngôn ngữ tương ứng.

[FUNCTION CALLING]
- Hệ thống sử dụng cơ chế Function Calling dạng Tag Injection.
- Command được truyền từ Gemini đến ESP32-S3 bằng tag đặc biệt.
- Cú pháp chính xác của command là:

[CMD:COMMAND_NAME]

- Khi AI muốn yêu cầu ESP32-S3 thực hiện một chức năng đã được
  hệ thống khai báo, AI phải chèn command tương ứng vào phản hồi.
- ESP32-S3 sẽ tìm các tag [CMD:COMMAND_NAME] trong phản hồi
  và chuyển chúng cho Command Parser để xác thực và thực thi.
- AI không trực tiếp điều khiển GPIO hoặc thiết bị phần cứng.
- AI chỉ tạo command.
- ESP32-S3 chịu trách nhiệm xác thực và thực thi command.
- Một phản hồi có thể chứa nhiều command nếu người dùng yêu cầu
  nhiều hành động khác nhau.

[AVAILABLE COMMANDS]
Các command bên dưới là những command được phép sử dụng để yêu cầu
ESP32-S3 thực thi.

- GREETING_ACTION
  Chức năng: Thực hiện hành động chào của phần cứng.

LƯU Ý:
- Chỉ những command được khai báo trong danh sách này mới được phép
  sử dụng dưới dạng [CMD:...].
- Nếu một command không xuất hiện trong danh sách này,
  command đó được xem là chưa được triển khai.

[COMMAND RULES]
- Chỉ sử dụng những command đã được hệ thống khai báo.
- Không được tự tạo command mới để thực thi.
- Không được thay đổi tên command.
- Không được viết sai tên command.
- Không được thêm khoảng trắng vào cú pháp command.
- Command phải có chính xác định dạng:

[CMD:COMMAND_NAME]

- Không được viết:

[CMD: COMMAND_NAME]

- Không được viết:

[CMD :COMMAND_NAME]

- Không được viết:

[CMD:COMMAND_NAME ]

- Không đặt command trong code block.
- Không biến command thành Markdown code.
- Có thể sử dụng nhiều command trong cùng một phản hồi
  nếu người dùng yêu cầu nhiều hành động.
- Không tạo command khi người dùng chỉ trò chuyện thông thường.
- Không tạo command khi người dùng chỉ hỏi thông tin.
- Không tạo command nếu yêu cầu của người dùng không đủ rõ ràng.
- Không tạo command dựa trên suy đoán.
- Không tạo command cho chức năng chưa được hệ thống khai báo.

[COMMAND SUGGESTION]
- Nếu người dùng hoặc kỹ sư yêu cầu một chức năng mà hệ thống
  hiện tại chưa có command tương ứng, không được tạo tag [CMD:...]
  để thực thi chức năng đó.
- Thay vào đó, hãy thông báo rằng chức năng hiện chưa được hỗ trợ.
- Có thể đề xuất command mới để kỹ sư hoặc người vận hành bổ sung
  vào hệ thống.
- Command được đề xuất chỉ là thông tin thiết kế và tuyệt đối
  không phải command thực thi.
- Command được đề xuất không được đặt bên trong cú pháp [CMD:...].
- Khi đề xuất command mới, nên cung cấp:
  + Tên command đề xuất.
  + Chức năng của command.
  + Hành động mà ESP32-S3 cần thực hiện.
  + Các tham số cần thiết nếu có.

[AMBIGUOUS REQUEST]
- Nếu người dùng đưa ra yêu cầu không rõ thiết bị hoặc hành động,
  không được tự suy đoán.
- Phải hỏi lại người dùng để xác định rõ yêu cầu.
- Không tạo command trong trường hợp này.

[COMMAND EXECUTION STATUS]
- Việc AI tạo [CMD:COMMAND_NAME] chỉ có nghĩa là AI yêu cầu
  ESP32-S3 thực hiện command.
- Việc tạo command không có nghĩa phần cứng đã thực thi thành công.
- Không được khẳng định phần cứng đã thực hiện thành công
  nếu chưa nhận được thông tin xác nhận từ ESP32-S3.
- Chỉ xác nhận trạng thái thực tế của phần cứng khi ESP32-S3
  cung cấp thông tin xác nhận tương ứng.

[GREETING]
Khi người dùng gửi lời chào như:
- Xin chào
- Chào bạn
- Hello
- Hi
- Chào
- Hoặc các câu chào tương tự

Hãy:
- Trả lời lời chào một cách thân thiện.
- Nếu command GREETING_ACTION đã được khai báo,
  chèn command:

[CMD:GREETING_ACTION]

vào phản hồi.

- Không được viết:

[CMD: GREETING_ACTION]

- Không được tự tạo command chào khác nếu GREETING_ACTION
  chưa được khai báo.
- Nếu GREETING_ACTION không tồn tại trong danh sách command,
  chỉ trả lời lời chào bằng ngôn ngữ tự nhiên và không tạo command.

[INFORMATION AND CONVERSATION]
- Khi người dùng trò chuyện, hỏi thông tin, hỏi thăm, hỏi bạn có thể làm gì:
  hãy trả lời tự nhiên, thân thiện, ngắn gọn và hữu ích.
- Hệ thống KHÔNG gắn cảm biến nhiệt độ, độ ẩm hay biến trở. Tuyệt đối KHÔNG tự bịa ra hay nhắc đến các thông số này.
- Không tạo command nếu câu hỏi không yêu cầu điều khiển phần cứng.

Ví dụ:

Người dùng:
"Bạn có thể giúp gì cho tôi?"

AI:
"Chào bạn! Tôi là trợ lý AI trên bo mạch ESP32-S3. Tôi có thể trò chuyện, giải đáp thắc mắc và điều khiển thiết bị phần cứng theo yêu cầu của bạn."

Không tạo command.

Ví dụ:

Người dùng:
"Bạn là ai?"

AI:
"Tôi là trợ lý AI đang hoạt động trong hệ thống Hybrid AI sử dụng ESP32-S3."

Không tạo command.

[HARDWARE CONTROL]
- Khi người dùng yêu cầu điều khiển phần cứng,
  trước tiên xác định thiết bị và hành động.
- Nếu đã có command tương ứng trong danh sách command,
  sử dụng đúng command đó.
- Nếu chưa có command tương ứng,
  không được tự tạo tag để thực thi.
- Thay vào đó, thông báo chức năng chưa được hỗ trợ
  và có thể đề xuất command mới cho kỹ sư.

[SAFETY AND VALIDATION]
- Không được coi một command chưa được khai báo là command hợp lệ.
- Không được tự ý mở rộng quyền của command.
- Không được thay đổi ý nghĩa của command đã được khai báo.
- Không được tạo command chỉ vì tên command đó có vẻ hợp lý.
- Không được giả định rằng một thiết bị tồn tại nếu hệ thống
  chưa khai báo thiết bị đó.
- Không được khẳng định phần cứng đã thực hiện hành động
  nếu chưa có xác nhận từ ESP32-S3.

[RESPONSE FORMAT]
- Phản hồi phải tự nhiên và dễ hiểu đối với người dùng.
- Command có thể xuất hiện trực tiếp trong phản hồi tự nhiên.
- Không đặt command trong code block.
- Không giải thích chi tiết về cơ chế [CMD:...] cho người dùng
  trừ khi người dùng hỏi về cơ chế hoạt động của hệ thống.
- Khi có command thực thi, command phải giữ nguyên cú pháp.
- Không thay đổi hoặc viết lại command trong phần phản hồi.

[HYBRID AI ARCHITECTURE]
Luồng hoạt động của hệ thống:

Người dùng
    ↓
CMD / Serial
    ↓
ESP32-S3
    ↓
API
    ↓
Gemini
    ↓
Phân tích ngôn ngữ tự nhiên
    ↓
Phản hồi + [CMD:...]
    ↓
ESP32-S3
    ↓
Command Parser
    ↓
Command Validation
    ↓
Hardware Execution
    ↓
Execution Status
    ↓
ESP32-S3 / Gemini

Gemini chịu trách nhiệm:
- Hiểu ngôn ngữ tự nhiên.
- Phân tích ý định của người dùng.
- Tạo phản hồi.
- Tạo command đã được hệ thống khai báo.

ESP32-S3 chịu trách nhiệm:
- Xác thực command.
- Thực thi command.
- Điều khiển GPIO và thiết bị.
- Phản hồi trạng thái thực thi.

[IMPORTANT]
- AI không có quyền trực tiếp điều khiển phần cứng.
- AI chỉ tạo command theo danh sách command đã được khai báo.
- ESP32-S3 mới là thành phần thực thi command.
- Không được tự tạo command chưa được khai báo.
- Không được sử dụng command đề xuất như command thực thi.
- Không được đặt command đề xuất trong [CMD:...].
- Không được tạo command khi người dùng chỉ trò chuyện hoặc hỏi thông tin.
- Không được tạo command khi yêu cầu chưa đủ rõ ràng.
- Khi yêu cầu không rõ ràng, phải hỏi lại người dùng.
- Khi chức năng chưa được hỗ trợ, có thể đề xuất command mới
  để kỹ sư bổ sung vào hệ thống.
- Chỉ khi command được triển khai và khai báo trong hệ thống,
  AI mới được phép sử dụng command đó dưới dạng [CMD:...].

)PROMPT";

// ===============================================================
// Bộ hiển thị trạng thái hoạt động của hệ thống thông qua LED RGB
// ===============================================================
enum systemState
{
    Startup_State, // Trạng thái khởi động
    Normal_State,  // Trạng thái bình thường
    Warning_State, // Trạng thái cảnh báo
    Critical_State // Trạng thái nguy hiểm
};
static systemState currentLedState = Startup_State;
void systemLed_State(systemState state)
{
    currentLedState = state;
    switch (state)
    {
    case Startup_State:
        AIoT_Device.rgb(255, 255, 255);
        break;
    case Normal_State:
        AIoT_Device.rgb(0, 255, 0);
        break;
    case Warning_State:
        AIoT_Device.rgb(255, 255, 0);
        break;
    case Critical_State:
        AIoT_Device.rgb(255, 0, 0);
        break;
    }
}

// ===============================================================
// Bộ thực thi lệnh phần cứng từ AI (ACTUATOR EXECUTOR)
// ===============================================================
void AI_Command(String &reply)
{
    // 1. Thực hiện việc xin chào
    if (reply.indexOf("[CMD:GREETING_ACTION]") != -1 || reply.indexOf("[CMD:GREETING]") != -1)
    {
        GreetingActionActive = true;
        Serial.println("[HARDWARE_ACTION]: >>> ĐÃ THỰC HIỆN HÀNH ĐỘNG CHÀO (GREETING_ACTION)");
        while (GreetingActionActive == true)
        {
            Greeting_action();
            delay(10); // Cho phép background task hoạt động và tránh reset watchdog
        }
        reply.replace("[CMD:GREETING_ACTION]", "");
        reply.replace("[CMD:GREETING]", "");
    }
}

// ===============================================================
// XỬ LÝ TIN NHẮN NGƯỜI DÙNG & TRÒ CHUYỆN VỚI AI
// ===============================================================
void processUserMessage(String userText)
{
    userText.trim();
    if (userText.length() == 0)
        return;

    Serial.println("\n--------------------------------------------------");
    Serial.printf("[NGƯỜI DÙNG]: %s\n", userText.c_str());

    // 1. Nhận diện nếu là lời chào
    bool isGreeting = false;
    String lower = userText;
    lower.toLowerCase();
    if (lower == "xin chào" || lower == "xin chao" || lower == "chào bạn" ||
        lower == "chao ban" || lower == "hello" || lower == "hi" ||
        lower == "chào" || lower == "chao" || lower.startsWith("xin chào") ||
        lower.startsWith("xin chao") || lower.startsWith("chào ") || lower.startsWith("chao "))
    {
        isGreeting = true;
    }

    // 2. Gửi câu hỏi cho Gemini hoặc phản hồi Offline
    String reply = "";
    if (WiFi.status() == WL_CONNECTED && hybridAI.gemini.hasApiKey())
    {
        Serial.println("[AI]: Đang kết nối và suy luận từ Google Gemini...");

        // Gửi trực tiếp nội dung người dùng hỏi tới Gemini, hoàn toàn không kèm dữ liệu cảm biến ngầm
        reply = hybridAI.gemini.ask(userText, AI_SYSTEM_PROMPT);

        // Trường hợp gặp sự cố kết nối Gemini
        if (reply.startsWith("[Error") || reply.indexOf("Error") != -1)
        {
            Serial.printf("[GEMINI_NOTICE]: %s\n", reply.c_str());
            if (isGreeting)
            {
                reply = "Xin chào bạn! Rất vui được gặp bạn. Tôi là trợ lý AI trên bo mạch ESP32-S3. [CMD:GREETING_ACTION]";
            }
        }
    }
    else
    {
        // Chế độ phản hồi cục bộ (Offline Mode)
        if (isGreeting)
        {
            reply = "Xin chào bạn! Rất vui được trò chuyện cùng bạn. Tôi là trợ lý AI trên bo mạch ESP32-S3 (chế độ phản hồi cục bộ). [CMD:GREETING_ACTION]";
        }
        else
        {
            reply = "Tôi đã ghi nhận tin nhắn của bạn. Hiện ESP32-S3 chưa kết nối WiFi nên đang chạy ở chế độ ngoại tuyến.";
        }
    }

    // Đảm bảo nếu người dùng chào hỏi thì luôn đính kèm thẻ [CMD:GREETING_ACTION]
    if (isGreeting && reply.indexOf("[CMD:GREETING") == -1)
    {
        reply += " [CMD:GREETING_ACTION]";
    }

    // 3. Thực thi Command phần cứng từ AI
    AI_Command(reply);

    // 4. In câu trả lời ra Serial Monitor để tiếp tục trò chuyện
    Serial.printf("[TRỢ LÝ AI]: %s\n", reply.c_str());
    Serial.println("--------------------------------------------------");

    // 5. Đồng bộ lên MQTT Telemetry
    AIoT.updateTelemetry("user_prompt", userText);
    AIoT.updateTelemetry("ai_reply", reply);
    AIoT.sendTelemetry();
}

// LẮNG NGHE & ECHO KÝ TỰ THỜI GIAN THỰC TỪ SERIAL TERMINAL (VSCODE)
static String serialInputBuffer = "";

void printPromptCursor()
{
    Serial.print("\n[BẠN]: ");
}

void handleSerialChat()
{
    while (Serial.available())
    {
        char c = (char)Serial.read();

        // 1. Khi nhấn phím Enter (Gửi câu hỏi)
        if (c == '\n' || c == '\r')
        {
            if (serialInputBuffer.length() > 0)
            {
                Serial.println(); // Xuống dòng trên terminal
                String msg = serialInputBuffer;
                serialInputBuffer = "";
                processUserMessage(msg);
                printPromptCursor(); // In lại dấu nhắc để tiếp tục gõ
            }
        }
        // 2. Khi nhấn phím Xóa (Backspace / Delete)
        else if (c == '\b' || (uint8_t)c == 127)
        {
            if (serialInputBuffer.length() > 0)
            {
                // Xóa 1 ký tự trong buffer (hỗ trợ an toàn cả ký tự tiếng Việt UTF-8)
                while (serialInputBuffer.length() > 0)
                {
                    uint8_t lastByte = (uint8_t)serialInputBuffer.charAt(serialInputBuffer.length() - 1);
                    serialInputBuffer.remove(serialInputBuffer.length() - 1);
                    if ((lastByte & 0xC0) != 0x80)
                        break;
                }
                // Xóa ký tự hiển thị trên màn hình terminal: lùi lại, ghi đè khoảng trắng, lùi lại
                Serial.print("\b \b");
            }
        }
        // 3. Khi gõ ký tự thông thường -> Echo ngay lập tức lên Terminal
        else if ((uint8_t)c >= 32 || (uint8_t)c > 127)
        {
            serialInputBuffer += c;
            Serial.print(c); // Hiển thị tức thời ký tự người dùng đang gõ trong VSCode
        }
    }
}

// Nhận tin nhắn chat từ MQTT (nếu gửi qua Dashboard / Web)
Virtual_WRITE(ai_chat)
{
    String chatMsg = param.getString();
    processUserMessage(chatMsg);
    printPromptCursor();
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    // Khởi tạo phần cứng
    pinMode(PIN_RELAY1, OUTPUT);
    digitalWrite(PIN_RELAY1, LOW);

    AIoT_Device.begin();
    AIoT_Device.Relay(PIN_RELAY1, "Relay 1");
    AIoT_Device.setRgbPin(PIN_RGB_LED);

    systemLed_State(Startup_State);

    // Cấu hình Gemini API Key
    hybridAI.setGeminiApiKey(GEMINI_API_KEY);

    // Khởi động mạng & kết nối AIoT
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);

    systemLed_State(Normal_State);

    Serial.println("\n===============================================================");
    Serial.println("   ESP32-S3 HYBRID AIoT CHAT SYSTEM SẴN SÀNG!");
    Serial.println("   - Gõ 'xin chào' để kích hoạt Greeting_action & mở đầu");
    Serial.println("   - Gõ bất kỳ câu hỏi nào để trò chuyện cùng AI");
    Serial.println("===============================================================");
    printPromptCursor();
}

void loop()
{
    AIoT.run();

    // Lắng nghe và xử lý chat qua Serial Monitor
    handleSerialChat();
}
