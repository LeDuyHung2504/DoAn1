#include <SoftwareSerial.h>
SoftwareSerial mySerial(4, 5);

// Khai báo chân kết nối
const int windSensorPin = 2;  // Chân tín hiệu của cảm biến gió
volatile int pulseCount = 0;  // Biến đếm xung (được cập nhật trong ISR)

// Thời gian và tần suất đo
unsigned long lastMeasurementTime = 0;
const unsigned long measurementInterval = 1000;  // 1 giây (1000 ms)

// Tốc độ gió
float WindSpeed = 0.0;

// Hàm xử lý ngắt
void countPulse() {
  pulseCount++;
}

void setup() {
  // Khởi tạo cổng nối tiếp
  Serial.begin(9600);
  mySerial.begin(9600);

  // Cấu hình chân cảm biến
  pinMode(windSensorPin, INPUT_PULLUP);

  // Gán ngắt vào chân cảm biến
  attachInterrupt(digitalPinToInterrupt(windSensorPin), countPulse, FALLING);
}

void loop() {
  unsigned long currentTime = millis();

  // Nếu đủ khoảng thời gian đo khoảng 1s, chốt số vòng / s
  if (currentTime - lastMeasurementTime >= measurementInterval) {
    noInterrupts();                      // Tạm thời dừng ngắt
    int currentPulseCount = pulseCount;  // Lưu giá trị xung đếm được
    pulseCount = 0;                      // Đặt lại bộ đếm
    interrupts();                        // Kích hoạt lại ngắt

    //    thông số cảm biến:
    //    1 vòng/s = 20 xung/s = 1,75m/s
    // => 1x/s = 0.0875m/s
    WindSpeed = 0.0875*currentPulseCount;
    // In kết quả ra Serial Monitor
    Serial.print("Tốc độ gió (m/s): ");
    Serial.println(WindSpeed);

    // Cập nhật thời gian đo
    lastMeasurementTime = currentTime;
  }
      //Gửi tín tốc độ gió đến esp8266
    Send_WS();
}

void Send_WS()
{
  mySerial.println(WindSpeed, 2); // Gửi dữ liệu đến ESP8266 
  delay(1000);
}



// encoder có 20 xung 
// con encoder nó phải ngắt liên tục để nhận đủ xung
// nếu dùng chung vào esp thì nó sẽ liên tục không có thời nhận dữ liệu từ các cảm biến khác, 

// tách riêng uno đo, gửi tín dữ sang esp, gửi bằng chuẩn UART


