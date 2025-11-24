## Mục tiêu 

## Phần cứng sử dụng


## 1. CS TRONG SPI
```c
#define CS_LOW(PORT,PIN)   ((PORT)->BSRR = (1 << ((PIN)+16)))
#define CS_HIGH(PORT,PIN)  ((PORT)->BSRR = (1 << (PIN)))
```
- CS_LOW → ghi vào BSRR (bit PIN+16) → RESET bit → pin = 0
→ Kéo chân CS xuống mức LOW
- CS_HIGH → ghi vào BSRR (bit PIN) → SET bit → pin = 1
-> Kéo chân CS lên mức HIGH

- SPI thông thường dùng CS active-low giống như LED trên PC13
- Chuẩn SPI tiêu chuẩn:
- CS = 0 → Slave được chọn
- CS = 1 → Thả Slave
```c
void CS_Select(GPIO_TypeDef* port, uint8_t pin)
{
    CS_LOW(port,pin);
}

void CS_Deselect(GPIO_TypeDef* port, uint8_t pin)
{
    CS_HIGH(port,pin);
}
```

**GPIO_TypeDef** là struct mô tả toàn bộ thanh ghi của 1 port GPIO
- GPIO_TypeDef mô tả layout hardware → code đọc/ghi chính xác vào đúng offset
- Địa chỉ ngoại vi hardware được ánh xạ vào RAM → MCU đọc ghi trực tiếp vào địa chỉ
- Dùng con trỏ để trỏ được đến mọi GPIO

## 2. SPI1_Init()
- Bật clock cho GPIOA , AF (SPI cần để gửi đến MCU) , SPIEN, DMA1EN
- Cấu hình PA5=SCK, PA6=MISO, PA7=MOSI
- SCK và MOSI: MCU phát tín hiệu → phải là output AF push-pull
- MISO: Slave trả dữ liệu → MCU phải là input floating

**2.1 Cấu hình SPI1 CR1**
```c
SPI1->CR1 =
      SPI_CR1_MSTR   // Chế độ Master
    | SPI_CR1_SSM    // Tự quản lý chân NSS bằng phần mềm
    | SPI_CR1_SSI    // Đặt NSS = 1 trong nội bộ (tránh mode fault)
    | SPI_CR1_SPE    // Bật SPI
    | (0x3 << 3);    // Baudrate = Fpclk / 16
```
- SPI_CR1_MSTR : MCU là Master. Nếu quên bit này → SPI đứng im không chạy.
- SPI_CR1_SSM : cho phép MCU tự quản lý chân NSS.

- Không dùng SSM (hardware NSS): PA4 được SPI điều khiển → chỉ 1 slave → rất hạn chế
- Dùng SSM (software NSS) : → Master tự kéo từng chân CS → điều khiển hàng chục slave SPI : PA3 = CS Flash ,PA8 = CS LCD ,... 

- SPI_CR1_SSI : → ép NSS nội bộ = 1 để tránh lỗi MODF (mode fault)
-> Nếu ta bỏ quên SSI → SPI bật lên là lỗi ngay.
- SPI_CR1_SPE : Bật SPI
- (0x3 << 3) = BaudRate = Fpclk / 16 -> Baudrate thực tế phụ thuộc APB2 clock.

- SPI cần 4 chân, nhưng NSS không phải lúc nào cũng dùng
- Vì ta chọn SSM + SSI → NSS vật lý không cần thiết
- Trong trường hợp nhiều slave -> BẮT BUỘC (để chọn từng cái)
- Trong trường hợp DMA / high-speed : Thường dùng chân GPIO riêng làm CS

## 2.2 SPI1_TransferByte()

**1. Xóa RX buffer**
```c
while (SPI1->SR & SPI_SR_RXNE)
{
    dummy = SPI1->DR;      // doc de clear RXNE
}
(void)dummy;
```
- RXNE = 1 nghĩa là có dữ liệu cũ trong RX buffer. Nếu không đọc, byte mới khi gửi sẽ gây Overrun (OVR).
- Dùng dummy = SPI1->DR; để “hút” dữ liệu cũ, **tránh OVR khi gửi byte mới”
- OVR chỉ xảy ra nếu RX buffer chưa đọc hết
- volatile dùng ở dummy để tránh trình biên dịch tối ưu

**2. Đợi TXE rồi ghi dữ liệu**
```c
    timeout = SPI_TIMEOUT;
    while (SPI1->SR & SPI_SR_BSY)
    {
        if (--timeout == 0)
            return 0xFF;       // loi: SPI dang ban
    }

    timeout = SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_TXE))
    {
        if (--timeout == 0)
            return 0xFE;       // loi: TXE timeout
    }

    SPI1->DR = data;           // ghi byte can gui
```
- TXE=1 không đảm bảo byte trước đó đã clock ra ngoài.
- Chỉ là TX buffer đã trống, Shift register có thể vẫn đang gửi
- TXE=1 không đồng nghĩa BSY=0 , ta cần check BSY , BSY=1 nghĩa là SPI đang truyền dữ liệu trước đó.


**2. Ghi vào DR luôn sinh ra một vòng TX + RX**
```c
SPI1->DR = data;           // ghi byte can gui

    /* -------------------------------------------------------
       4) Cho den khi byte nhan ve ton tai trong RXNE
       ------------------------------------------------------- */
    timeout = SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_RXNE))
    {
        if (--timeout == 0)
            return 0xFD;       // loi: RXNE timeout
    }

    uint8_t recv = SPI1->DR;   // doc byte da nhan
```

- SPI1->DR = data;
- **SPI bắt đầu clock 8 bit ra MOSI , SPI cùng lúc clock 8 bit từ MISO vào**
- Lưu ý : Dù ta chỉ “gửi” dữ liệu, ta luôn nhận về 1 byte rác nếu không dùng.
- Đọc SPI1->DR để clear
- Nếu không đọc DR → buffer đầy -> sẽ kẹt vì RXNE luôn = 1 → **SPI không thể nhận byte tiếp theo** -> SPI stuck
- Khi đó đồng nghĩa với muốn gửi byte tiếp theo cũng không ra được MOSI.

**4. Timeout giúp code không bị treo chết**
- Không có timeout → nếu SPI treo (clock không chạy, chip chết, dây nối sai) → while() chạy mãi.
- SPI_TIMEOUT bảo vệ CPU không bị treo

**5. Kiểm tra Overrun (OVR)**
```c
if (SPI1->SR & SPI_SR_OVR)
{
    dummy = SPI1->DR;  
    dummy = SPI1->SR;  
    (void)dummy;
    return 0xFC;
}
```
- OVR xảy ra khi RX buffer chưa đọc mà byte mới đã nhận.
- Phải đọc DR rồi đọc SR để clear flag OVR.
- Đọc SR trước DR không clear OVR, nên thứ tự **DR → SR** là quan trọng

**6 Đảm bảo BSY=0 khi kết thúc**
```c
timeout = SPI_TIMEOUT;
while (SPI1->SR & SPI_SR_BSY)
{
    if (--timeout == 0)
        return 0xFB;
}
```
- Đảm bảo shift register đã hoàn tất truyền bit cuối cùng
- Khi dùng nhiều SPI liên tiếp, BSY=1 mà gửi tiếp → byte cũ bị ngắt quãng

**7. Lưu ý quan trọng**
- SPI có 3 buffer khác nhau:
1. TX buffer → TXE kiểm tra cái này
2. Shift register → thực sự đẩy từng bit lên MOSI
3. RX buffer → RXNE kiểm tra cái này
- Khi ta ghi DR:
- Data vào TX buffer
- Lập tức nạp vào Shift Register
- Khi shift xong → byte vào RX buffer
- RXNE=1
-> **TXE=1 nhưng RXNE chưa =1 → byte vẫn chưa gửi xong!**
- Nếu ta gửi lệnh và cần delay đúng thì chờ RXNE là cách chính xác nhất
- Byte nhận được là do slave xuất ra, không liên quan byte gửi.

## 2.3 SPI1_TransferBuffer()

- là hàm truyền một mảng dữ liệu qua SPI1 tới một slave nhất định, và nhận dữ liệu trả về theo kiểu polling.
- txBuf → dữ liệu cần gửi
- rxBuf → mảng nhận dữ liệu (có thể NULL nếu không quan tâm dữ liệu trả về)
- len → số byte cần truyền
- csPort / csPin → chân chip select (slave nào được chọn)
  
**1.Chọn slave trước khi truyền**
- CS_Select(csPort,csPin);
- Kéo CS xuống LOW → Slave được chọn
- Lưu ý : Nếu quên kéo CS xuống → Slave không nhận dữ liệu → SPI truyền vô ích
- **CS phải kéo LOW trước byte đầu tiên, và phải thả HIGH sau byte cuối cùng**
  
**2.Vòng lặp truyền từng byte**
```c
for(uint16_t i=0; i<len; i++)
{
    uint8_t data = SPI1_TransferByte(txBuf[i]);
    if(rxBuf) rxBuf[i] = data;
}
```
- SPI1_TransferByte(txBuf[i]) : Gửi 1 byte và chờ phản hồi từ slave 
- if(rxBuf) rxBuf[i] = data; 
- Nếu rxBuf != NULL → lưu byte nhận được
- Nếu rxBuf == NULL → bỏ qua dữ liệu nhận → tiết kiệm RAM và tránh crash

**3. Thả CS sau khi truyền xong**
- CS_Deselect(csPort,csPin);
- Kéo CS HIGH → Slave thả
- Đây là chuẩn SPI, nhiều IC chỉ latch dữ liệu khi CS HIGH
- Lưu ý : nhiều IC nhạy với thời gian CS (tối thiểu 10ns – 1µs), nếu CS lên quá nhanh → slave không kịp ghi dữ liệu, cần delay nhỏ nếu dùng tốc độ cao

**4. Lưu ý quan trọng**

- Hàm này polling, CPU hoàn toàn bị chặn trong vòng lặp
- Ở dự án lớn,ta chuyển sang DMA để giảm tải CPU 
- Nếu txBuf và rxBuf là NULL hoặc không cùng độ dài → phải kiểm tra
- Nếu có nhiều slave, không dùng global SPI1_TransferByte trực tiếp, mà thêm mutex / semaphore nếu dùng RTOS
- rxBuf NULL nhưng code vẫn ghi -> Crash, bus lỗi
- Truyền buffer dài, polling dài -> DMA
- Gọi SPI1_TransferBuffer trong ISR -> Sai, vì polling chặn quá lâu → hệ thống treo

## 2.4 DMA1_Channel2_IRQHandler()
```c
void DMA1_Channel2_IRQHandler(void)  // RX
{
    if(DMA1->ISR & DMA_ISR_TCIF2)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF2;
        spiTransferDone = 1;
    }
}
```
- **Interrupt Handler** cho DMA1 Channel 2, tức DMA nhận dữ liệu (RX) từ SPI1.
- Khi DMA hoàn tất truyền hết dữ liệu (transfer complete), nó gọi ngắt.
- các công việc : Kiểm tra cờ hoàn tất (TCIF2), Xóa cờ để ngắt không gọi lại liên tục, Đặt cờ spiTransferDone = 1 để main biết dữ liệu đã sẵn sàng
- Đây là “báo hiệu truyền xong” cho SPI DMA RX
**1. void DMA1_Channel2_IRQHandler(void)**
- Hàm được gọi tự động khi DMA1 channel 2 có sự kiện ngắt
- Tên hàm phải đúng theo vector table STM32 (startup_stm32f10x.s)
- if(DMA1->ISR & DMA_ISR_TCIF2) // DMA1->ISR là register trạng thái ngắt của tất cả kênh DMA1.
- DMA_ISR_TCIF2 = bit báo Transfer Complete Interrupt Flag cho Channel 2
- Dòng này kiểm tra: DMA channel 2 đã truyền xong chưa.
- Lưu ý : Nếu Không kiểm tra cờ này → handler có thể gọi nhầm khi cờ khác set.

**2. DMA1->IFCR = DMA_IFCR_CTCIF2;**
- IFCR = Interrupt Flag Clear Register
- Ghi CTCIF2 = Clear Transfer Complete Flag for channel 2
- Nếu không clear → handler sẽ bị gọi lại liên tục
  
**3. spiTransferDone = 1;**
- Đây là biến global, báo cho main loop hoặc hàm DMA polling rằng dữ liệu đã sẵn sàng đọc.
- Biến này nên khai báo volatile để compiler không tối ưu, vì nó thay đổi trong ISR
- Nếu không khai báo volatile, compiler có thể “giả sử” giá trị không đổi trong main loop → bug rất khó nhận ra
  
**4. Check TEIF**
- TEIF = transfer error , TEIF = báo lỗi transfer, không phải lỗi MCU
- Chỉ clear flag trong IRQ để ngăn IRQ lặp vô hạn. TEIF chỉ là báo lỗi, còn việc ISR nhảy là do TEIE + TEIF = 1 hoặc cả hai
- Thiết kế  = 1 LÀ nhảy vào ngắt là phòng trường hợp bị lỗi thì NVIC kịp thời phát hiện 
- TEIF = báo lỗi
- TEIE = bật/tắt khả năng nhảy ISR khi TEIF = 1
- Xử lý nặng, reset, báo lỗi thực hiện ngoài IRQ (main loop / task) , ví dụ thêm dmaRxErrorFlag = 1; chẳng hạn


**5.Lưu ý quan trọng**
- Xử lý trong ISR không được dùng vòng lặp hoặc delay.
- Chỉ set cờ hoặc đọc/ghi thanh ghi → nhanh → tránh blocking
- Vì sao không dùng NVIC_DisableIRQ ở đây : Vì ngắt này rất ngắn → không cần disable IRQ , chỉ disable IRQ nếu thao tác phức tạp.

## 2.5 DMA1_Channel3_IRQHandler() 
```c
void DMA1_Channel3_IRQHandler(void)  // TX
{
    if(DMA1->ISR & DMA_ISR_TCIF3)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF3;
    }
}
```
- DMA1 Channel 3 thường được dùng truyền dữ liệu (TX) của SPI1
- Khi DMA TX hoàn tất truyền buffer → cờ TCIF3 set → NVIC gọi handler này
- Hàm này xử lý các việc : 
- Kiểm tra cờ Transfer Complete Flag (TCIF3)
- Xóa cờ bằng IFCR
- KHÔNG cần set flag báo main loop, vì main loop chỉ quan tâm RX DMA, TX DMA full-duplex → TX xong đồng thời RX cũng xong.

**Lưu ý quan trọng**

- ISR chỉ có check cờ + clear cờ → nhanh → không cần disable NVIC.
- Nếu ISR dài → cần NVIC_DisableIRQ để tránh race condition.
- DMA TCIF vs TEIF : TCIF = transfer complete → bình thường
- TEIF = transfer error → check TEIF để báo lỗi.
- Nếu buffer lớn, có thể dùng half-transfer interrupt (HTIF) → xử lý buffer nửa đầu nửa sau để tối ưu CPU.



## 2.6 SPI1_TransferBuffer_DMA()
- Hàm này làm truyền dữ liệu SPI nhiều byte bằng DMA, kết hợp:
1. **CS Select/ Deselect** để chọn slave.
2. **TX DMA** để gửi dữ liệu từ buffer txBuf đến SPI1->DR.
3. **RX DMA** để nhận dữ liệu từ SPI1->DR vào buffer rxBuf
4. **NVIC IRQ** để xử lý interrupt DMA khi hoàn tất
5. **Wait loop** để chờ RX DMA xong, với timeout để tránh treo.
6. **Tắt DMA & thả CS** khi kết thúc.

**1. Reset cờ và kéo CS xuống**
```c
spiTransferDone = 0;
spiError = 0;

CS_Select(csPort,csPin);
```
- spiTransferDone và spiError là volatile global
- Reset trước khi truyền, để main loop biết trạng thái mới
- CS_Select kéo CS xuống 0, chọn slave → bắt đầu SPI transmission
- Nếu kéo CS sai thời điểm, slave có thể bỏ byte đầu tiên → lỗi dữ liệu.

**2. Cấu hình TX DMA**
```c
SPI_DMA_TX_CH->CCR = 0;            // reset config
SPI_DMA_TX_CH->CPAR = (uint32_t)&(SPI1->DR); // Peripheral addr = SPI DR
SPI_DMA_TX_CH->CMAR = (uint32_t)txBuf;      // Memory addr = txBuf
SPI_DMA_TX_CH->CNDTR = len;                  // số byte cần truyền
SPI_DMA_TX_CH->CCR |= (1 << 0)   // EN
                    | (1 << 1)   // TCIE
                    | (1 << 6)   // MINC
                    | (1 << 7);  // DIR
```
- bit 0 (EN) : Bật DMA channel
- bit 1 (TCIE) : Cho phép ngắt Transfer Complete
- bit 6 (MINC) : Memory Increment → sau mỗi byte, pointer memory tăng
- bit 7 (DIR) :  1 = memory → peripheral (TX)
- Nếu không bật MINC, tất cả byte sẽ ghi vào cùng 1 DR → dữ liệu bị lặp
- Nếu không bật TCIE, ISR TX vẫn chạy nhưng main loop không biết trạng thái TX

**3. Cấu hình RX DMA**
```c
SPI_DMA_RX_CH->CCR = 0;
SPI_DMA_RX_CH->CPAR = (uint32_t)&(SPI1->DR);
SPI_DMA_RX_CH->CMAR = (uint32_t)rxBuf;
SPI_DMA_RX_CH->CNDTR = len;
SPI_DMA_RX_CH->CCR |= (1 << 0)   // EN
                    | (1 << 1)   // TCIE
                    | (1 << 6);  // MINC
```
- Bit DIR = 0 (mặc định) → Peripheral → memory
- RX DMA bắt buộc MINC, nếu không, tất cả byte đọc vào cùng 1 ô → mất dữ liệu.
- RX DMA là quan trọng nhất để main loop biết hoàn thành, vì SPI full-duplex, TX đi đồng thời RX về.

**4. Bật DMA trong SPI**
- SPI1->CR2 |= SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
- Nếu quên, DMA sẽ chạy nhưng SPI không tự động lấy dữ liệu → TX/RX không hoạt động
  
**5. Bật NVIC cho ngắt DMA**
```c
NVIC_EnableIRQ(DMA1_Channel2_IRQn);
NVIC_EnableIRQ(DMA1_Channel3_IRQn);
```
- RX DMA = Channel 2 → ISR sẽ set spiTransferDone = 1
- TX DMA = Channel 3 → ISR chỉ clear cờ
- Nếu quên NVIC, DMA vẫn chạy, nhưng ISR không gọi → spiTransferDone không set → loop chờ treo
  
**6. Chờ RX DMA hoàn thành**
```c
uint32_t timeout = SPI_TIMEOUT*10;
while(!spiTransferDone && !spiError)
    if(--timeout==0){ spiError=1; break; }
```
- spiTransferDone được ISR RX set = 1
- Timeout = bảo vệ tránh treo nếu DMA lỗi
- Không chờ RX → dữ liệu chưa đầy đủ → main loop đọc buffer bị lỗi
- Timeout = “safety net” trong sản xuất, tốt hơn dùng semaphore/RTOS event
  
**7. Tắt DMA và thả CS** 
```c
SPI1->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
CS_Deselect(csPort,csPin);
```
- Tắt DMA → tránh ISR không cần thiết
- CS_Deselect kéo CS lên 1 → kết thúc giao tiếp slave
- luôn tắt DMA trước khi thả CS, nếu không, slave có thể nhận thêm byte rác
  
**8. Lưu ý quan trọng**
- CS phải kéo trước TX DMA enable
- CS phải thả sau khi DMA tắt
- Sai thứ tự → slave bỏ byte đầu hoặc nhận byte thừa
- DMA MINC : nếu không, pointer memory không tăng → tất cả dữ liệu vào/ra cùng 1 địa chỉ
  
 
## 2.7 SCK
**1. SCK điều khiển thời điểm dữ liệu hợp lê**

- SCK = Serial Clock – xung nhịp do master tạo ra để điều khiển việc truyền dữ liệu giữa master và slave.
- Dữ liệu của SPI không hợp lệ liên tục, mà chỉ hợp lệ tại cạnh lên hoặc cạnh xuống của SCK (tùy mode).
- SPI có 4 mode dựa vào CPOL và CPHA
  
**2. CPOL và CPHA**
- CPOL = 0 → SCK nhàn rỗi ở mức thấp (Idle = Low)
- CPOL = 1 → SCK nhàn rỗi ở mức cao (Idle = High)
- CPHA = 0 → Sample (lấy dữ liệu) ở cạnh đầu tiên
- CPHA = 1 → Sample ở cạnh thứ hai
- ![CPOL%20_CPHA](./png/CPOL%20_CPHA.PNG)

**3. SCK quyết định tốc độ truyền SPI** 
- SCK càng cao → tốc độ SPI càng nhanh
- Không phải slave nào cũng chạy được SPI nhanh → nếu SCK quá cao → lỗi, mất bit, treo chip

**4. SCK đồng bộ hóa 2 chiều: MISO và MOSI**
- MOSI: Master gửi → Slave đọc theo SCK
- MISO: Slave gửi → Master đọc theo SCK
- Không có SCK → không có dữ liệu đi lại

**5. SCK là tín hiệu duy nhất master tạo trong mọi tình huống**
- Ngay cả khi master nhận dữ liệu (RX only) → vẫn phải có SCK
- Slave KHÔNG BAO GIỜ tự tạo clock
- SCK Điều khiển mọi hoạt động trong SPI (SPI sống nhờ SCK)

**6. SCK liên quan chặt đến DMA trong SPI**
- Nếu SPI không enable **TXDMAEN/RXDMAEN** hoặc không bật SPI → Không có SCK → DMA đứng im → không truyền gì. 

**7. Các vẫn đề Lưu ý**
- Nhiễu trên đường SCK = lỗi SPI ngẫu nhiên
- Dây SCK dài quá → méo xung -> cần giảm tốc độ hoặc thêm trở 33–100Ω
- Một số slave chỉ nhận dữ liệu khi SCK có duty chuẩn 50% (Thời gian HIGH = 50% chu kỳ ,Thời gian LOW = 50% chu kỳ )
  
## 2.8 Vòng lặp chính
- Hàm thực hiện 4 việc chính :
- Khởi tạo toàn bộ phần cứng (LED, SPI1, chân CS)
- Run SPI polling (truyền từng byte, đợi từng byte)
- Run SPI DMA (truyền toàn bộ buffer bằng DMA, không tốn CPU)
- So sánh dữ liệu nhận → báo LED trạng thái

**1.Khởi tạo hệ thống**
```c
LED_Init();
SPI1_Init();
CS_Init(SPI1_CS1_PORT,SPI1_CS1_PIN);
CS_Init(SPI1_CS2_PORT,SPI1_CS2_PIN);
```
- Bật clock, cấu hình GPIO, cấu hình SPI1.
- Hai chân CS điều khiển hai “thiết bị slave SPI khác nhau”
  
**2. Khai báo buffer TX/RX**
```c
static uint8_t tx1[8] = {...};
static uint8_t rx1[8] = {0};
static uint8_t tx2[8] = {...};
static uint8_t rx2[8] = {0};
```
- Tại sao phải dùng Static ? Vì địa chỉ nằm trong vùng .data/.bss, DMA mới truy cập được ổn định.
- Nếu để trong stack → stack thay đổi → DMA đọc sai địa chỉ → lỗi nặng

**3. SPI POLLING TRANSFER**
```c
memset(rx1,0,sizeof(rx1));
SPI1_TransferBuffer(tx1,rx1,8,SPI1_CS1_PORT,SPI1_CS1_PIN);
```
- memset(rx1,0...) → reset buffer nhận về 0 để dễ check kết quả.
- SPI1_TransferBuffer() : truyền từng byte 1 bằng tay, không dùng DMA / CPU bận chờ TXE và RXNE
- Polling SPI luôn chạy “full-duplex” → mỗi byte gửi = nhận lại 1 byte.
- CPU bị block trong lúc polling → nếu dùng RTOS cần tránh

**4.SPI DMA TRANSFER**
```c
spiTransferDone = 0;  // bắt buộc
spiError = 0;         // bắt buộc
```
- DMA RX khi hoàn thành sẽ set spiTransferDone = 1 trong ISR trước đó
- Nếu ta không reset trước → DMA chưa chạy mà flag đã = 1 →
→ chương trình NHẦM rằng DMA đã xong → sai logic → lỗi cực khó debug
```c
SPI1_TransferBuffer_DMA(tx2,rx2,8,SPI1_CS2_PORT,SPI1_CS2_PIN);
```
- DMA sẽ:
- lấy con trỏ tx2
- lấy con trỏ rx2
- truyền 8 byte
- tự động điều khiển SPI
- tự động copy data từ SPI1->DR vào RAM
- gọi ngắt RX DMA khi xong
 
**5. Lưu ý**
- DMA SPI luôn full-duplex : Ta truyền 8 byte, ta bắt buộc cũng nhận 8 byte.
- DMA TX ISR KHÔNG quan trọng : chỉ RX ISR mới quyết định truyền hoàn tất
- static buffer cho DMA là yêu cầu gần như bắt buộc : DMA không hoạt động tốt với biến local trong stack.
- Busy-wait trong while(1) KHÔNG được dùng trong thực tế , mà dùng semaphre , event , queue ,..

## 2.9 Lưu ý
- MISO phải có điện trở pull-up/pull-down tùy Slave : Nếu dây dài hoặc nhiễu → MISO có thể floating → đọc rác.
- SPI1 tốc độ tối đa phụ thuộc tốc độ slave -> Không phải cứ chỉnh BR nhỏ nhất là chạy được
- DMA chỉ hoạt động khi SPI TXE/RXNE được cấu hình đúng , Clock DMA bật nhưng chưa cấu SPI thành DMA → DMA không chạy
- Mode (CPOL/CPHA) không cấu ở đây → mặc định mode 0 -> Nhiều sensor cần mode 1 hoặc mode 3

## Ảnh chụp 

