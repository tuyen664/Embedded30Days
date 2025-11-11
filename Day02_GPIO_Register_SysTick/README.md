## Mục tiêu bài học:
Cấu hình Led nháy chu kì chính xác 1ms dùng SysTick


## Phần cứng sử dụng:
** STM32F103C8T6 
**  Led  nối chân PC13


## Giải thích code hoạt động : 

1. **Cấu hình GPIO** 

- Bật clock cho port C (RCC->APB2ENR |= 0x10)
- Cấu hình chân **PC13** là **Output Push-Pull, tốc độ 50 MHz**
- GPIOC->ODR : Output Data Register (thanh ghi dữ liệu đầu ra)
Nếu bit đó = 1 → chân xuất ra mức HIGH (3.3V) -> led tắt
Nếu bit đó = 0 → chân xuất ra mức LOW (0V) -> led sáng 

2. **Cấu hình SysTick**

- cấu hình SysTick chạy mỗi 1ms kích hoạt cờ count , giảm 1ms (giảm biến u32Delay)

  SysTick->LOAD = 72 * 1000 - 1; // đếm 71999 -> 0 (24bit)
  Hệ thống chạy ở 72MHz (1s chạy 72.10^6 xung clock)-> chạy chu kì 72000 xung clock là 1ms

  SysTick->VAL=0U; // Val là biến đếm đang chạy nó chạy từ Load-> 0 , đến 0 thì bật COUNTFLAG
  Và reset lại 
- SysTick->CTRL = 5U;  
                bit 0 : Enable SysTick
		bit 1 : TICKINT : bat ngat
		bit 2 : CLKSOURCE : 1 -> (CLOCK CPU)HCKL = 72MHz , 0 = HCKL/8
		bit 16: COUNTFLAG : Co bao dem xong (auto clear khi doc)
     → Bật SysTick, dùng clock CPU, không dùng ngắt

3. **Vòng lặp chính** 

- Mỗi khi đủ 1000 ms (1 giây), chương trình **đảo trạng thái bit PC13**
- GPIOC->ODR ^= (1 << 13);
 → LED sẽ **bật/tắt luân phiên mỗi giây**


## Ảnh chụp 