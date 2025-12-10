## STM32F1 Embedded 30-Day Learning Projects (C, HAL, Bare-metal)

## Mục Tiêu 

- Hiểu rõ kiến thức nền tảng Embedded C
- Sử dụng được các ngoại vi thiết yếu: GPIO, Timer, ADC, PWM, UART, I2C, SPI, DMA…
- Xây dựng project , Debug và cải tiến
- Quen với Git/GitHub, viết tài liệu kỹ thuật và quay video demo

## Tổng quan nội dung 30 ngày 
- Ngày 1-8     : GPIO , Interrupt (SysTick , Timer , EXTI) , Debug cơ bản
- Ngày 9-12    : UART , ADC , DMA
- Ngày 13-14   : PWM , SERVO
- Ngày 15- 17  : Project tổng hợp
- Ngày 18-19   : LCD , I2C
- Ngày 20      : SPI
- Ngày 23-29   : FreeRTOS , Interrupt trong FreeRTOS ,...
- Ngày 30      : CAN
- Ngày 31- ..  : Ôn tập

 **Mỗi thư mục chứa:**
  
- Source code
- Kiến thức lý thuyết và giải thích từng dòng lệnh
- video demo

## Cách build & chạy
1. Clone repo: git clone https://github.com/tuyen664/Embedded30Days/tree/main
2. Mở bằng Keil C (nếu dùng MakeFile thì sửa dòng #include "stm32f1x.h" thành #include "stm32f1xx.h")
3. Chọn Debug bằng ST-link
4. Build → Flash → Test trên board STM32F103C8T6

