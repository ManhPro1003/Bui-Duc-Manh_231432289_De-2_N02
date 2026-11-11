# 🌱 ESP32 FreeRTOS MQTT – Hệ thống giám sát môi trường vườn cây công nghiệp

<p align="center">
  <strong>IoT Garden Monitoring and Control System</strong><br>
  <em>Giám sát nhiệt độ, độ ẩm và điều khiển thiết bị cho vườn cà phê, hồ tiêu</em>
</p>

---

## 📌 1. Giới thiệu dự án

Dự án xây dựng một mô hình **giám sát và điều khiển môi trường vườn cây công nghiệp** như cà phê, hồ tiêu, sử dụng vi điều khiển **ESP32**, hệ điều hành thời gian thực **FreeRTOS** và giao thức truyền thông **MQTT**.

ESP32 thu thập dữ liệu nhiệt độ, độ ẩm từ cảm biến DHT22, sau đó gửi dữ liệu lên hệ thống giám sát thông qua MQTT. Người dùng có thể theo dõi thông số môi trường trên Website và điều khiển máy bơm, quạt từ giao diện. Ngoài ra, hệ thống còn có chức năng điều khiển phun sương tự động dựa trên các ngưỡng độ ẩm được thiết lập.

Điểm trọng tâm của đề tài là tổ chức chương trình theo mô hình đa nhiệm bằng FreeRTOS. Các công việc đọc cảm biến, truyền dữ liệu và điều khiển phun sương được phân chia thành những Task riêng, phối hợp với nhau thông qua Queue và Mutex.

## 🎯 2. Mục tiêu

- Thu thập và theo dõi nhiệt độ, độ ẩm môi trường bằng cảm biến DHT22.
- Truyền dữ liệu từ ESP32 lên Website thông qua giao thức MQTT.
- Cho phép người dùng bật/tắt máy bơm và quạt từ giao diện Website.
- Tự động điều chỉnh mức phun sương theo độ ẩm đo được.
- Ứng dụng FreeRTOS để tổ chức các tác vụ chạy đồng thời và trao đổi dữ liệu an toàn.
- Mô phỏng, kiểm tra hoạt động của hệ thống trước khi triển khai trên phần cứng thực tế.

## 🧰 3. Công nghệ và linh kiện sử dụng

| Thành phần | Vai trò |
|---|---|
| ESP32 | Vi điều khiển trung tâm, đọc cảm biến và điều khiển thiết bị |
| DHT22 | Đo nhiệt độ và độ ẩm môi trường |
| FreeRTOS | Quản lý các tác vụ chạy đồng thời trên ESP32 |
| MQTT | Trao đổi dữ liệu và lệnh điều khiển giữa ESP32 với hệ thống giám sát |
| EMQX Public Broker | Máy chủ trung gian MQTT: `broker.emqx.io` |
| Website | Hiển thị dữ liệu và gửi lệnh điều khiển thiết bị |
| Wokwi | Môi trường mô phỏng mạch và chương trình ESP32 |
| ArduinoJson | Phân tích và tạo dữ liệu JSON trao đổi qua MQTT |
| PubSubClient | Thư viện kết nối và giao tiếp MQTT trên ESP32 |

### Thư viện Arduino cần thiết

- `WiFi.h`
- `PubSubClient.h`
- `ArduinoJson.h`
- `DHT.h`
- Các thư viện FreeRTOS được cung cấp trong môi trường ESP32 Arduino Core.

## 🔌 4. Sơ đồ kết nối phần cứng

| Thiết bị | Chân ESP32 | Chức năng |
|---|---:|---|
| DHT22 – DATA | GPIO 4 | Đọc nhiệt độ và độ ẩm |
| Máy bơm – tín hiệu điều khiển | GPIO 18 | Bật/tắt máy bơm |
| Quạt – tín hiệu điều khiển | GPIO 19 | Bật/tắt quạt |
| Phun sương – tín hiệu PWM | GPIO 23 | Điều chỉnh mức công suất phun sương |

> **Lưu ý:** Trong mô phỏng, các đầu ra có thể được biểu diễn bằng LED hoặc linh kiện thay thế để quan sát trạng thái. GPIO chỉ là tín hiệu điều khiển; khi triển khai thiết bị công suất thực tế cần sử dụng mạch driver, MOSFET, relay hoặc module phù hợp. Không nối trực tiếp tải công suất vào chân GPIO của ESP32.

## ⚙️ 5. Kiến trúc và nguyên lý hoạt động

### 5.1. Luồng hoạt động tổng quát

1. ESP32 khởi tạo cảm biến DHT22, các chân GPIO, Queue, Mutex và các Task FreeRTOS.
2. ESP32 kết nối Wi-Fi bằng mạng mô phỏng `Wokwi-GUEST`.
3. ESP32 kết nối MQTT Broker tại `broker.emqx.io` qua cổng `1883`.
4. Task đọc cảm biến lấy dữ liệu nhiệt độ, độ ẩm theo chu kỳ 2 giây.
5. Dữ liệu được đưa vào Queue để Task truyền thông gửi lên MQTT Broker.
6. Website nhận dữ liệu telemetry và cập nhật các thông số hiển thị.
7. Khi người dùng thao tác nút máy bơm hoặc quạt trên Website, lệnh MQTT được gửi đến ESP32. ESP32 xử lý lệnh và cập nhật trạng thái đầu ra.
8. Task điều khiển phun sương nhận dữ liệu độ ẩm mới nhất và điều chỉnh đầu ra PWM theo ngưỡng cài đặt.

### 5.2. Phân chia các Task FreeRTOS

| Task | Core | Nhiệm vụ chính |
|---|---:|---|
| `Task_ReadData` | 1 | Đọc DHT22 mỗi 2 giây, hiển thị dữ liệu qua Serial và gửi dữ liệu vào các Queue |
| `Task_Transmit` | 0 | Kết nối Wi-Fi/MQTT, xử lý MQTT, nhận dữ liệu từ `dataQueue` và gửi telemetry lên Broker |
| `Task_Alert` | 1 | Nhận độ ẩm mới nhất từ `dataAlert`, xác định ngưỡng và điều khiển phun sương |

Các Task phối hợp thông qua hai Queue:

- **`dataQueue`**: lưu các cấu trúc `SensorData` gồm nhiệt độ, độ ẩm và thời điểm lấy mẫu để chuyển dữ liệu từ Task đọc cảm biến sang Task truyền thông. Queue có độ dài 10 phần tử.
- **`dataAlert`**: lưu giá trị độ ẩm mới nhất để Task điều khiển phun sương xử lý. Queue có độ dài 1 và sử dụng `xQueueOverwrite()` để thay thế giá trị cũ bằng giá trị mới nhất.

Hai Mutex được sử dụng để hạn chế truy cập đồng thời:

- **`serialMutex`**: bảo vệ thao tác ghi dữ liệu ra Serial.
- **`actuatorMutex`**: bảo vệ các biến trạng thái của thiết bị như máy bơm, quạt và mức phun sương.

### 5.3. Điều khiển phun sương tự động

Hệ thống xác định mức phun sương dựa trên độ ẩm đo được từ DHT22:

| Độ ẩm môi trường | Mức phun sương | Thời gian cho chu kỳ |
|---|---:|---:|
| Từ 35% đến dưới 40% | 25% | 10 giây |
| Từ 30% đến dưới 35% | 50% | 20 giây |
| Dưới 30% | 100% | 30 giây |
| Từ 40% trở lên | 0% – Tắt | Tắt phun sương |

Task `Task_Alert` tiếp tục theo dõi độ ẩm trong quá trình phun. Khi độ ẩm đạt từ 40% trở lên, hệ thống dừng phun sương. Nếu độ ẩm chuyển sang vùng ngưỡng khác, mức điều khiển được cập nhật tương ứng.

Đầu ra PWM tại GPIO 23 được ánh xạ từ mức công suất 0–100% sang giá trị duty 0–255.

## 📡 6. Cấu hình Wi-Fi và MQTT

### 6.1. Kết nối

| Thông số | Giá trị trong chương trình |
|---|---|
| Wi-Fi SSID | `Wokwi-GUEST` |
| Wi-Fi Password | Để trống trong môi trường Wokwi |
| MQTT Broker | `broker.emqx.io` |
| MQTT Port | `1883` |
| MQTT Client ID | `esp32_lab3_cafe_4729` |

### 6.2. Các MQTT Topic

| Topic | Chiều truyền | Nội dung |
|---|---|---|
| `lab3/garden/cafe_4729/telemetry` | ESP32 → Broker/Website | Dữ liệu nhiệt độ, độ ẩm và thời điểm lấy mẫu |
| `lab3/garden/cafe_4729/control/pump/set` | Website → ESP32 | Lệnh bật/tắt máy bơm |
| `lab3/garden/cafe_4729/control/fan/set` | Website → ESP32 | Lệnh bật/tắt quạt |
| `lab3/garden/cafe_4729/status/actuators` | ESP32 → Broker/Website | Trạng thái máy bơm, quạt và mức phun sương |

### 6.3. Định dạng dữ liệu JSON

**Dữ liệu cảm biến (telemetry):**

```json
{
  "temperature": 28.5,
  "humidity": 36.0,
  "sampleTime": 123456
}
```

**Lệnh điều khiển máy bơm hoặc quạt:**

```json
{
  "state": true
}
```

Trong đó `true` tương ứng bật và `false` tương ứng tắt thiết bị.

**Trạng thái các thiết bị (actuator status):**

```json
{
  "pump": false,
  "fan": true,
  "mistPercent": 25
}
```

Thông báo trạng thái thiết bị được gửi với cờ retained để Website có thể nhận được trạng thái gần nhất khi đăng ký theo dõi topic.

## 🖥️ 7. Giao diện giám sát và điều khiển

Website đảm nhiệm hai nhóm chức năng chính:

- **Giám sát:** nhận dữ liệu MQTT và hiển thị nhiệt độ, độ ẩm môi trường.
- **Điều khiển:** cung cấp nút bật/tắt máy bơm và quạt; đồng thời nhận trạng thái thiết bị được ESP32 gửi lên Broker.

Các hình ảnh giao diện và kết quả mô phỏng được lưu trong thư mục `Images/` (nếu có trong Repository).

## ▶️ 8. Hướng dẫn chạy mô phỏng

1. Mở dự án mô phỏng Wokwi hoặc tạo mạch theo bảng kết nối tại mục 4.
2. Mở chương trình ESP32 và kiểm tra các thư viện cần thiết đã được khai báo.
3. Giữ cấu hình Wi-Fi `Wokwi-GUEST` khi chạy trên Wokwi.
4. Kiểm tra Broker, cổng MQTT, Client ID và các topic trong chương trình.
5. Chạy mô phỏng, mở Serial Monitor để quan sát dữ liệu cảm biến và trạng thái kết nối.
6. Mở Website giám sát, đăng ký đúng các topic telemetry/status để nhận dữ liệu.
7. Thử thay đổi giá trị cảm biến để kiểm tra chức năng tự động phun sương.
8. Thử các nút điều khiển máy bơm và quạt trên Website để kiểm tra chiều truyền lệnh từ Website đến ESP32.

> Website cần sử dụng đúng Broker và topic đã cấu hình trong chương trình ESP32. Mã nguồn Website không được mô tả chi tiết ở đây nếu không nằm trong các tệp của Repository.

## 📁 9. Cấu trúc Repository đề xuất

```text
ESP32-FreeRTOS-MQTT-Garden-Monitoring/
│
├── Source_Code/       # Mã nguồn ESP32 và tệp mô phỏng
├── Report/            # Báo cáo Word/PDF
├── Images/            # Hình ảnh mạch, Website và kết quả mô phỏng
└── README.md          # Giới thiệu và hướng dẫn dự án
```

Tên thư mục có thể thay đổi theo cách bạn sắp xếp các tệp thực tế trên GitHub.

## 📊 10. Kết quả và hướng phát triển

### Kết quả thực hiện

- Đọc và hiển thị dữ liệu nhiệt độ, độ ẩm từ DHT22.
- Truyền dữ liệu cảm biến lên MQTT Broker theo định dạng JSON.
- Nhận lệnh MQTT để điều khiển máy bơm và quạt.
- Gửi trạng thái hoạt động của các thiết bị lên Broker.
- Điều khiển mức phun sương tự động theo các ngưỡng độ ẩm đã thiết lập.
- Phân chia công việc thành các Task FreeRTOS và sử dụng Queue, Mutex để phối hợp hoạt động.

### Hướng phát triển

- Lưu trữ dữ liệu cảm biến để theo dõi lịch sử và phân tích xu hướng môi trường.
- Bổ sung biểu đồ nhiệt độ, độ ẩm theo thời gian trên Website.
- Mở rộng số lượng cảm biến và khu vực giám sát.
- Thử nghiệm trên phần cứng thực tế, sử dụng mạch điều khiển công suất phù hợp cho máy bơm, quạt và thiết bị phun sương.
- Bổ sung cơ chế xác thực, bảo mật và kết nối MQTT an toàn khi triển khai ngoài môi trường mô phỏng.

## ⚠️ 11. Lưu ý khi sử dụng

- Dự án hiện sử dụng MQTT Broker công cộng `broker.emqx.io`; topic có thể được các thiết bị khác sử dụng hoặc theo dõi. Nên đổi hậu tố `cafe_4729` thành mã riêng khi chạy nhiều nhóm hoặc triển khai thực tế.
- Không đưa mật khẩu Wi-Fi cá nhân, Device Secret, Access Token hoặc thông tin nhạy cảm lên Repository Public.
- Cổng MQTT `1883` trong cấu hình này là kết nối không mã hóa. Khi triển khai thực tế, cần cân nhắc xác thực và giao thức bảo mật phù hợp.
- Mô hình mô phỏng dùng để kiểm tra luồng xử lý và logic điều khiển; thiết bị phun sương, máy bơm và quạt thực tế cần phần cứng công suất và biện pháp an toàn riêng.

## 👨‍🎓 12. Thông tin học phần

- **Đề tài:** Thiết kế hệ thống giám sát dữ liệu môi trường vườn cây công nghiệp (cà phê, hồ tiêu).
- **Nền tảng:** ESP32 – FreeRTOS – MQTT.
- **Giảng viên hướng dẫn:** Thầy Võ Quang Sơn.
- **Mục đích:** Bài tập thực hành vận dụng kiến thức về hệ điều hành thời gian thực và Internet vạn vật (IoT).

---

<p align="center">
  <strong>ESP32 • FreeRTOS • MQTT • Internet of Things</strong><br>
  <em>Ứng dụng IoT trong giám sát và điều khiển môi trường nông nghiệp.</em>
</p>
