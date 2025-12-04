## Mục tiêu

- Nháy LED 3 lần, dừng 1 giây, lặp lại.

## Giải thích :

- PC13: Output push-pull 50MHz
- Dùng SysTick để tạo delay 1ms
- Delay_Ms(200): thời gian sáng/tắt LED
- Dùng biến **count** để đếm số lần led nháy , lưu ý cho led **nháy-tắt** làm 1 lần đếm để đồng bộ với count
  
  -> tránh khi count đã đếm về 0 nhưng led chưa ngáy đủ 3 lần.

## Lưu ý :

``` GPIOC->BSRR = (1 << (13 + 16)); // reset PC13 ``` -> LED on
``` GPIOC->BSRR = (1 << 13) ; // led off ```

- Chỉ có BSRR (32bit) mới có chức năng này 
- BRR(16bit) không có , chỉ có GPIOC->BRR = (1 << 13); // led sáng

## Video

https://drive.google.com/file/d/1VaXhMV9LhQRVAT9JmiwpKmZ9XYB_kqYz/view?usp=drive_link
