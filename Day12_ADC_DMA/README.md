## Mục tiêu 

 - Dùng ADC đa kênh + DMA để đo đồng thời trên 3 kênh (PA0, PA1, PA2).
 - Xuất giá trị điện áp (V) ra UART (Tera Term).

## Tổng quát 

 - ADC đo điện áp tại chân PAx được (0…Vref). STM32F1 dùng ADC 12-bit → giá trị 0…4095 tương ứng 0…Vref.
 - Luồng dữ liệu : Analog @PAx → ADC (DR) → DMA copy → adc_buf[] (RAM) → CPU đọc → chuyển thành Volt → in qua UART

## Giải thích 

### 1. Cấu hình ADC
 - Bật xung clock cho GPIOA VÀ ADC1 

 - Chia clock ADC (ADCPRE):
 - RCC->CFGR |= (0x2U << 14); // ADCPRE = 10 → PCLK2/6 → 72/6 = 12 MHz (≤ ~14 MHz khuyến nghị)
 - Dùng tốc độ cao hơn vẫn chạy , độ chính xác giảm.

 - Cấu hình các chân PA0–PA2 là  analog input (chỉ cần reset chúng về 0)
 - Lưu ý : ADC_CH0 = 0, ADC_CH1 = 1, ADC_CH2 = 2

 - Cấu hình thời gian lấy mẫu của ADC : SMPR2 (Sample time register) quyết định thời gian lấy mẫu ADC.
 - ADC1->SMPR2 |=  (0x7U << (3 * ADC_CH1)) ; // Mỗi kênh dùng 3 bit để chọn sample cycles.0x7 = 111 → chọn 239.5 ADC cycles (lâu nhất, phù hợp nguồn có R lớn)
 
 - ADC1->CR2 |= (1U << 0);              // ADON = 1 (wake up)
 - Chờ 1 ms để ADC ổn định : delay_ms(1);

 - Reset lại giá trị calibration cũ và chờ bit tự clear : 
 - ADC1->CR2 |= (1U << 3);              // RSTCAL = 1
 - while (ADC1->CR2 & (1U << 3));       // Wait until reset complete

 - Bắt đầu calibration ADC và chờ bit CAL trở về 0 → ADC đã sẵn sàng.
 
 - **Tóm tắt**
 - Bật clock cho GPIOA và ADC1.
 - Chia clock ADC xuống còn PCLK2/6.
 - Cấu hình PA0–PA2 là analog input.
 - Cài đặt thời gian lấy mẫu 239.5 cycles.
 - Bật ADC, chờ ổn định.
 - Reset calibration.
 - Chạy calibration.
 - ADC sẵn sàng. 

### 2. Cấu hình ADC khi dùng DMA
 - static void ADC1_DMA_ConFig(void);
 - SCAN mode = 1 → ADC sẽ đọc nhiều kênh liên tiếp theo thứ tự định nghĩa trong SQRx
 - ADC1->CR1 |= (1U << 8);           // SCAN = 1
 - Nếu SCAN = 0 → chỉ đọc 1 kênh duy nhất lặp đi lặp lại.

 - CONT = 1 → ADC sẽ liên tục chuyển đổi, không cần SWSTART mỗi lần : ADC1->CR2 |= (1U << 1); // CONT = 1
 - ADC1->CR2 |= (1U << 8);  DMA = 1 → Chỉ cho phép ADC tạo DMA request; DMA chỉ thực hiện copy khi ADC có conversion hoàn tất (EOC).
  
 - ADC1->SQR1 |= (ADC_CHANNELS - 1U) << 20; // số kênh trong sequence = L + 1
 
 - ADC1->SQR3 = (ADC_CH1 << 0) | (ADC_CH2 << 5) | (ADC_CH3 << 10);
 - SQR3 lưu 3 rank đầu của sequence: rank1 = kênh đọc đầu tiên , rank2 = kênh đọc thứ 2 ,...
 - Mỗi rank dùng 5 bit để chọn kênh ADC, CH0 → rank1, CH1 → rank2, CH2 → rank3
 
 - Chọn nguồn kích hoạt ADC là 111 = SWSTART :  ADC1->CR2 |=  (0x7U << 17); 
 - ADC1->CR2 &= ~(1U<<20); // EXTTRIG = 0 (không dùng ngoại vi) 
 - Vì dùng continuous mode, ADC sẽ tự quét liên tục, nên SWSTART chỉ cần gọi 1 lần là ADC chạy.

 - **Tóm tắt**
 - Bật scan mode → đọc nhiều kênh liên tiếp
 - Bật continuous conversion → ADC chạy liên tục
 - Bật DMA request 
 - Cấu hình số kênh quét = ADC_CHANNELS.
 - Định nghĩa thứ tự các kênh trong sequence.
 - Dùng software trigger, tắt EXTTRIG 

### 3. Cấu hình DMA1 channel1
 - Phần “bắt tay” giữa ADC1 và DMA1 Channel 1 , giúp ADC tự động đẩy kết quả vào vùng nhớ mà không tốn CPU
 - Enable DMA1 clock : RCC->AHBENR |= (1U << 0);
 - Tắt channel trước config: DMA1_Channel1->CCR &= ~(1U << 0); // EN = 0 .

 - DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR; 
 -> Địa chỉ ngoại vi (Peripheral Address): nơi DMA sẽ tự động đọc dữ liệu khi ADC hoàn thành 1 conversion.
 - Dữ liệu mà ADC đọc được được ghi ở đây.


 - DMA1_Channel1->CMAR = (uint32_t)adc_buf;
 - Memory Address — nơi DMA **"cất"** dữ liệu lấy từ ADC1->DR .
 - adc_buf là mảng 3 phần tử , lưu kết quả của 3 kênh ADC
 - DMA sẽ tự động ghi lần lượt vào adc_buf[0], adc_buf[1], adc_buf[2] ,...

 - DMA1_Channel1->CNDTR = ADC_CHANNELS;
 - CNDTR (Number of Data to Transfer): số lượng DMA cần truyền.

 - DMA1_Channel1->CCR = 0; // xóa trước khi config
 - DMA1_Channel1->CCR |= (1U << 7); // CIRC = 1 -> Circular mode (DMA quay vòng buffer) -> cho phép lặp lại liên tục
 - DMA1_Channel1->CCR |= (1U << 5); // MINC = 1 (Memory increment)-> sau mỗi lần truyền, địa chỉ bộ nhớ tăng lên 
 - DMA1_Channel1->CCR |= (1U << 8); // PSIZE = 16-bit → phù hợp với ADC1->DR (12-bit ADC, 16-bit register)
 - DMA1_Channel1->CCR |= (1U << 10); // MSIZE = 16-bit -> khớp kiểu uint16_t adc_buf[]
 - DMA1_Channel1->CCR |= (0x3U << 12); // PL = 11 (very high priority)
 - DMA1_Channel1->CCR &= ~(1U << 4);  //  DIR = 0 -> đọc dữ liệu từ peripheral -> memory
  
 - Sau khi cấu hình xong, bật lại DMA channel : DMA1_Channel1->CCR |= (1U << 0); 
 
 -**Quá trình hoạt động**
 - ADC tạo DMA request và khi có dữ liệu ở ADC->DR  
 - DMA1 Channel1 phát hiện Có request và có dữ liệu ở ADC->DR   → copy giá trị từ ADC1->DR sang adc_buf
 - Sau khi lần lượt copy đủ 3 giá trị (CH0–CH2), quay vòng lại (CIRC) và lặp lại
 - CPU chỉ việc đọc adc_buf[]

### 4. ADC Bắt đầu Conversion  
 -**Lưu ý quan trọng là ADC chỉ có thể bắt đầu Conversion sau khi ADC và DMA đã được cấu hình & bật hoàn tất hết**

### 5. Vòng Lặp chính
 - Ta phải sắp xếp thứ thự chạy các hàm hợp lý , để tránh DMA đọc nhầm dữ liệu rác.
 - Cấu hình UART trước
 - Cấu hình ADC (clock + sample time + calibration)
 - Cấu hình ADC khi có DMA (chế độ scan + DMA)
 - Cấu hình DMA
 - Bật ADC lần 2 (ADON)
 - Bật SWSTART để bắt đầu quét liên tục

**- Tóm Tắt quá trình hoạt động**
 - Khi reset, CPU cấu hình ADC + DMA đầy đủ.
 - Khi bật bit DMA = 1, ADC cho phép DMA hoạt động, nhưng DMA chỉ thực sự chuyển dữ liệu khi ADC có kết quả trong DR.
 - Khi SWSTART = 1, ADC bắt đầu chuyển đổi theo thứ tự :
 - Đo kênh 0 → DMA copy vào adc_buf[0]
 - Đo kênh 1 → DMA copy vào adc_buf[1]
 - Đo kênh 2 → DMA copy vào adc_buf[2]
 - DMA hoạt động ở chế độ circular → buffer luôn được cập nhật liên tục.

 - Trong vòng while(1) CPU chỉ việc đọc adc_buf[], đổi sang Volt theo công thức
 - V = ADC_value × 3.3​ / 4095
 - Sau đó in ra màn hình Tera Term qua UART thông qua mảng **char msg[100];** 


## Ảnh chụp 

