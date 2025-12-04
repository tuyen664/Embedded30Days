## Mục tiêu 
 
  - Biết cách cấu hình USART, gửi một dòng tin lên Tera Term

## Giải thích
 
**1. Cấu hình USART**
  - Bật clock cho GPIOA (bit 2) VÀ USART (bit 14)
  - Cấu hình PA9 làm TX (chân gửi dữ liệu ra Tera Term) , PA10 làm RX (chân nhận dữ liệu)
    
     PA9 : Alternate Function Push-Pull , 50MHz (1011) : ```GPIOA->CRH |=  (0x0B<<4);```
      
     PA10 : Input Floating mode (0100) → ``` GPIOA->CRH |= (0x04<<8);```
    
  - Cấu hình Baudrate (tốc độ truyền) : USART1->BRR = 0x1D4C;
       
     Hoặc ```USART1->BRR = 72000000/9600;```  Tốc độ truyền 9600
    
  - USART1->CR1 = (1<<13) | (1<<3) | (1<<2);
    
     (1<<13) → bật USART - bit UE (USART Enable)
    
     (1<<3) → bật truyền dữ liệu (TX) - bit TE (Transmitter Enable)
    
     (1<<2) → bật nhận dữ liệu (RX)   - bit RE (Receiver Enable)

**2. Các hàm gửi kí tự**

 **2.1 void USART1_SendChar(char c)**
  - Kiểm tra xem có bận gửi dữ liệu không : ```while(!(USART1->SR & (1<<7)));```
    
    USART1->SR là Status Register
    
    Bit 7 là TXE (Transmit Data Register Empty)
    
    TXE = 1: Thanh ghi truyền DR trống → có thể nhận dữ liệu mới
    
    TXE = 0: Đang bận gửi dữ liệu cũ
    
     -> Chờ cho đến khi TXE = 1
  
   **USART1->DR = c;**
   
    Khi TXE = 1, gán dữ liệu c vào Data Register (DR)
    
    UART sẽ **tự động** truyền giá trị đó ra chân TX (PA9)
    
    Khi ghi xong, UART sẽ tự động **reset TXE = 0** và bắt đầu gửi bit ra đường truyền

 **2.2 void USART1_SendString()**
 
  - Gửi một chuỗi ký tự (string) qua USART1, bằng cách gửi từng ký tự một cho đến khi gặp ký tự ('\0')
  - while(*str)
    
      *str truy cập ký tự mà con trỏ str đang trỏ tới
    
       Khi gặp '\0', điều kiện while(*str) = false, vòng lặp kết thúc
    
  - USART1_SendChar(*str++)
    
   *str → lấy ký tự hiện tại trong chuỗi 
   
   USART1_SendChar(...) → gọi hàm để gửi ký tự qua UART
   
   str++ : tăng con trỏ sang ký tự kế tiếp trong chuỗi
 
  - USART1_SendString("HELLO!\r\n"); gửi chuỗi ra Tera Term

     *str = 'H'

     *(str+1) = 'E'

  - Lưu ý về con trỏ :
    
    char *str → con trỏ trỏ đến chuỗi ký tự
    
    *str → ký tự mà con trỏ đang trỏ tới
    
    str++ → tăng con trỏ sang ký tự tiếp theo
    
    *str++ → lấy ký tự hiện tại, rồi trỏ sang ký tự kế tiếp.

## Video

https://drive.google.com/file/d/1uZq0yKFUe4N7KZvOGeNX_iLPa_CYcotd/view?usp=drive_link






