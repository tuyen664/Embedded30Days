## Chức năng

- TIM3 được cấu hình tạo PWM 1 kHz trên PA6 (TIM3_CH1), duty ban đầu bằng 0% → LED tắt
- TIM2 được cấu hình tạo ngắt mỗi 1 ms 
- Mỗi lần vào ngắt TIM2, duty được tăng hoặc giảm dần từ 0 → 1000 → 0
- Giá trị duty được ghi vào TIM3->CCR1, làm LED sáng lên rồi mờ xuống theo chu kỳ 

## Giải thích 

**1. Cấu hình PWM trên PA6 (TIM3_CH1)**

- PA6 nằm trên TIM3 Channel 1 (CH1) -> cấu hình TIM3_CH1 ở chế độ PWM và bật chân PA6

- Bật clock cho GPIOA và TIM3
  
   RCC->APB2ENR |= (1 << 2);  // GPIOA
  
   RCC->APB1ENR |= (1 << 1);  // TIM3
  
   GPIOA->CRL |=  (0xB << 24);  PA6 - AF Push-Pull 50MHz (phù hợp để xuất tín hiệu PWM) 
 
- TIM3->PSC = 72-1; // 72MHz/72 = 1MHz
  
- TIM3->ARR = PWM_MAX -1U ; Với PWM_MAX = 1000 → ARR = 999
  
   PWM period = 1000 ticks , PWM frequency = 1 MHz / 1000 = 1 kHz
  
   PWM frequency = 72MHz / (PSC+1) / (ARR+1)

- TIM3->CCR1 = 0 ; // duty = 0 / 1000 = 0% (chưa bật xung cao)

- TIM3->CCMR1 |= (6 << 4) | (1 << 3); // CCMR1 (Capture/Compare Mode Register 1)

    (6<<4) → OC1M = 110 = PWM mode 1 (output HIGH khi CNT < CCR, LOW khi CNT ≥ CCR)
  
    (1<<3) → OC1PE = 1 bật preload cho CCR1 (CCR1 mới chỉ có hiệu lực sau update event, không cập nhật ngay lập tức)
  
     OC1M = 111 đảo pha (HIGH/LOW ngược lại)

- TIM3->CCER |= (1 << 0);
  
     (1 << 0) -> CC1E = 1 → enable capture/compare 1 output (cho phép xuất xung ra pin qua channel1)

- TIM3->CR1  |= (1 << 7) | (1 << 0);

   Bit 7 ARPE = 1 → Auto-reload preload enable. Giá trị mới viết vào ARR sẽ chỉ có hiệu lực sau một update event

   Bit 0 CEN = 1 → start counter (enable counter)
 
- TIM3->EGR |= (1 << 0);
  
   UG = 1 → generate update ngay lập tức

**2. Cấu hình TIM2 làm Interrupt**

- Enable clock cho TIM2
- Cấu hình prescaler (PSC) : ```TIM2->PSC = 7199;```  ( 72MHz / 7200 = 10 kHz)
- Cấu hình ARR (Auto Reload Register) : ```TIM2->ARR = 10 - 1;``` -> Timer tạo interrupt mỗi 1 ms (tần số 1000 Hz).
- Bật ngắt : ```TIM2->DIER |= (1 << 0);``` Cho phép TIM2 tạo ngắt khi đếm tràn (update event)
  
  -> Nếu không bật → TIM2 chạy nhưng ISR không bao giờ được gọi
  
- Start Timer : TIM2->CR1  |= (1 << 0); // Bit 0 = CEN (Counter Enable) 
- Bật ngắt TIM2 trong NVIC : NVIC_EnableIRQ(TIM2_IRQn);
  
  -> Mỗi 1ms gọi TIM2_IRQHandler() ;

**3. TIM2_IRQHandler()**

- ```if(TIM2->SR & (1 << 0))``` (bit UIF = 1 ) → kiểm tra lại ngắt có đúng của TIM2
- Xóa cờ ngắt : TIM2->SR &= ~(1 << 0); // Nếu không clear → ISR bị gọi liên tục

- Cập nhật duty cycle để tạo hiệu ứng sáng mờ : duty += direction * STEP;
  
    duty = giá trị PWM hiện tại (0 → PWM_MAX)
  
    direction = +1 hoặc -1
  
    STEP = lượng thay đổi mỗi lần

- ```if(duty >= PWM_MAX) { duty = PWM_MAX; direction = -1; }``` Nếu lên đến MAX → đổi hướng giảm
- ```else if(duty == 0)  { direction = 1; }```  Nếu giảm về 0 → đổi hướng tăng
- Tạo ra dạng sóng tam giác → đèn LED sáng dần → mờ dần lặp lại

- Cập nhật giá trị PWM mới : TIM3->CCR1 = duty;
- Khi duty thay đổi → độ rộng xung PWM thay đổi theo → độ sáng LED thay đổi. 


## Video

https://drive.google.com/file/d/1V8tmH7x7xgLd1hcW51gp4WMesbTxTuIN/view?usp=drive_link

