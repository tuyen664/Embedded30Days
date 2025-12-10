## Tổng quan
- Thiết kế chạy 2 task song song trên FreeRTOS:
  
   Task_LED : Nháy LED PC13 mỗi 500ms

   Task_UART : Gửi chuỗi UART1 mỗi 1 giây
  
- Scheduler : Người điều phối CPU và phân chia thời gian cho 2 task

## main() 
**1. SystemInit();**

- Set hệ thống clock lên 72MHz (qua PLL)
- Load vector table vào VTOR (quan trọng khi dùng interrupt với FreeRTOS)

**2. Khởi tạo GPIO và UART trước khi tạo Task**
```c
GPIO_Config();
UART1_Init(9600);
```
- FreeRTOS không đảm bảo code init sẽ chạy trước khi task dùng
- Nếu đặt init trong task thì dễ gây race condition

**3. Tạo 2 task cùng priority = 1**
```c
xTaskCreate(Task_LED, "LED", 128, NULL, 1, NULL);
xTaskCreate(Task_UART, "UART", 256, NULL, 1, NULL);
```
- Scheduler quyết định task nào được phép chạy trong từng thời điểm
- Scheduler chạy theo time slicing , Round robin nếu nhiều task có **cùng priority**

   Time slicing = chia nhỏ thời gian CPU thành các lát thời gian bằng nhau (time slice)

   Mỗi task được chạy trong một lát thời gian cố định

   Hết lát thời gian, Scheduler chuyển sang task kế tiếp theo vòng tròn
  
   Round Robin = quay vòng đều giữa các task có cùng priority
  
- Trong FreeRTOS thì  1 time slice = 1 tick (ta có thể quay đổi 1 tick ra các thời gian khác nhau)

  ->**Có nhiều task cùng priority + chúng đều Ready + configUSE_TIME_SLICING = 1 → RTOS sẽ chuyển task mỗi tick**

- Nếu configUSE_TIME_SLICING = 0 : Không round-robin. Task nào chạy thì cứ chạy, trừ khi nó tự block hoặc nhường.
- Ta dùng khi muốn mỗi task phải tự chịu trách nhiệm nhường CPU , hoặc đảm bảo task chạy đến đúng thời điểm block mà không bị tick ngắt.
- configUSE_TIME_SLICING = 0 chỉ làm mất round-robin giữa task cùng priority , đối với task có priority cao hơn READY thì task được chạy ngay.

**3.1**
- **Task_LED** : Đây là function pointer con trỏ đến hàm task.
- Task phải có dạng :
```c
void Task_LED(void *pvParameters);
```
- Mỗi task không bao giờ return , nếu return khỏi hàm → task crash, vì nó không có nơi để “trở về”
- FreeRTOS sẽ không bao giờ gọi trực tiếp Task_LED , mà restore context của task
  
- Nó tạo một context (stack + TCB(Task Control Block)) rồi nhảy vào hàm
  
- Một context bao gồm:
  
   Stack riêng của task : RTOS sẽ cấp RAM cho mỗi task (256 bytes…) , không dùng chung stack với main() hoặc task khác
  
   Mỗi tack có 1 cái **call stack** riêng để chứa : local variables , return address , saved registers khi chuyển context.

**TCB – Task Control Block** bao gồm : 
- Con trỏ stack hiện tại (SP) , priority , trạng thái task (Running / Ready / Blocked / Suspended)
- Thời gian delay , tên task , địa chỉ hàm task sẽ chạy , usage CPU
  
- Sau khi context tạo xong, scheduler sẽ: 

   Chọn task có priority cao nhất đang Ready

   Load stack pointer của task đó

   Pop các register

   CPU bắt đầu thực thi hàm task tại địa chỉ cung cấp


**3.2**
- **"LED"** – Tên task : Dùng cho debug (như qua FreeRTOS+Trace, Segger SystemView, hoặc CLI)
- Không ảnh hưởng tốc độ hay bộ nhớ , Không được quá dài → tốn RAM

**3.3**
- 128 – **stack size** , đơn vị stack là word (4 byte) : 128 x 4 = 512 bytes stack
- FreeRTOS không có bảo vệ stack — nên khi tràn stack, Task không chạy nhưng không báo lỗi
  
  -> cần có kiểm tra lỗi này

**3.4**
- **NULL – parameter cho task** :  Ta không truyền dữ liệu vào task, nên để NULL
- Ví dụ dùng parameter để Truyền địa chỉ struct chứa config GPIO pin , tái sử dụng 1 function cho nhiều task : 
```c
typedef struct {
    int pin;
    int interval;
} TaskConfig;

TaskConfig led1 = {13, 500};
TaskConfig led2 = {14, 1000};


xTaskCreate(TaskBlink, "LED1", 128, &led1, 2, NULL);
xTaskCreate(TaskBlink, "LED2", 128, &led2, 2, NULL);

void TaskBlink(void *pvParameters)
{
    TaskConfig *cfg = (TaskConfig*)pvParameters;
    while(1) {
        toggleGPIO(cfg->pin);
        vTaskDelay(cfg->interval);
    }
}
```
**3.5**
- **priority = 1** : Đây là độ ưu tiên của task(Priority = 1 >  0)
- Trong code có 2 task cùng priority = 1 → CPU chia đều , chạy luôn phiên

**3.6**
- **NULL – task handle** : Vì ta không cần điều khiển task sau khi tạo, nên để NULL
- Nếu muốn xóa task (vTaskDelete) , suspend hoặc resume → ta phải truyền handle vào đây
- Ví dụ : Tạo 2 task, task1 nháy LED, task2 điều khiển pause/resume task1
```c
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x.h"

TaskHandle_t Task1Handle = NULL;  // handle cho task1

void Task1(void *pvParameters)
{
    while(1) {
        // toggle LED
        GPIOC->ODR ^= (1<<13);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void Task2(void *pvParameters) {
    while(1)
   {
        vTaskDelay(pdMS_TO_TICKS(5000));  
        vTaskSuspend(Task1Handle);              // tạm dừng task1
        vTaskDelay(pdMS_TO_TICKS(3000)); 
        vTaskResume(Task1Handle);               // chạy lại task1
    }
}

int main(void)
{
    // init GPIOC pin13
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |= (0x1 << 20);

    xTaskCreate(Task1, "Blink", 128, NULL, 2, &Task1Handle);
    xTaskCreate(Task2, "Controller", 128, NULL, 1, NULL);

    vTaskStartScheduler();
    while(1);
}
```

**3.7 Lưu ý**
- xTaskCreate không bao giờ fail trừ khi thiếu RAM
- Nếu thiếu RAM heap của FreeRTOS → xTaskCreate trả về pdFAIL
  
   -> KHÔNG check error → hệ thống crash mà không hiểu tại sao
  
- Stack của task không phải stack của main :  Mỗi task có stack riêng → hoàn toàn tách biệt
- Task chạy ngay lập tức sau khi scheduler start -> FreeRTOS sẽ chọn task có priority cao nhất chạy đầu tiên
- Task_LED sẽ không bao giờ thoát ra khỏi for(;;) : Nếu thoát ra → Task bị xóa
- vTaskDelay() giúp nhường CPU : nếu không delay → Task_LED chiếm 100% CPU
- FreeRTOS dùng **heap FreeRTOS** , không phải heap của C


**4. vTaskStartScheduler();**
- Bắt đầu chạy Task → từ đây CPU chỉ chạy task, **main() không còn chạy nữa** 

##. Task_LED
```c
static void Task_LED(void *pvParameters)
{
    (void) pvParameters;
    for (;;)
    {
        GPIOC->ODR ^= (1 << 13);          // Toggle LED on PC13
        vTaskDelay(pdMS_TO_TICKS(500));   // Delay 500ms (non-blocking)
    }
}
```
- Dùng vTaskDelay giải phóng CPU cho task khác 
- pdMS_TO_TICKS(500) : chuyển 500ms -> 500 tick (dùng trong FreeRTOS) , FreeRTOS tính thời gian theo tick, không phải theo ms 
- Task đang chạy sẽ tạm dừng (Block) trong 500 ms và nhường CPU cho task khác , Nó không tạo delay kiểu “chặn CPU” như HAL_Delay()
 

## Task_UART
```C
UART1_SendString("Hello from FreeRTOS!\r\n");
vTaskDelay(pdMS_TO_TICKS(1000));
```
- Gửi UART theo từng character bằng polling (TXE)
  
   → Mỗi lần gửi 1 ký tự phải đợi TXE → chiếm CPU → không tối ưu
  
- Tuy nhiên vẫn an toàn vì Task gửi 1 lần mỗi giây → không nhiều


## USART1_Init
- PA9 (TX): AF push-pull 50MHz 
- PA10 (RX): floating input
```c
while (!(USART1->SR & USART_SR_TXE));
USART1->DR = (*str++ & 0xFF);
```
- TXE = transmit buffer empty
- Không có interrupt → Task UART sẽ block CPU khi gửi nhiều dữ liệu
- Trong dự án thật phải dùng UART + DMA

## Lưu ý quan trọng

**1. FreeRTOS không chạy task ngay sau xTaskCreate**
- Phải đợi vTaskStartScheduler() chạy xong SysTick
  
**2. vTaskDelay là delay tương đối, không phải delay tuyệt đối**
- Delay từ thời điểm hiện tại của task → là tương đối , không đảm bảo chu kỳ đều
- Nó sẽ block task ít nhất 500 ms, nhưng không đảm bảo task sẽ chạy lại đúng 500 ms vì còn phụ thuộc vào scheduler và các task khác có priority cao hơn
  
**2.1 Delay tuyệt đối (vTaskDelayUntil)**
```c
TickType_t xLastWakeTime = xTaskGetTickCount();

for(;;)
{
    // làm việc gì đó
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
}
```
- xLastWakeTime lưu tick lần cuối task chạy
- vTaskDelayUntil() sẽ delay đến đúng tick tiếp theo theo chu kỳ → chu kỳ task rất đều
- Còn vTaskDelay mỗi chu kì sẽ bị trễ thời gian đoạn code ban đầu
- vTaskDelayUntil() = delay tuyệt đối về tick count, nhưng không ép task chạy ngay
- Nếu task khác priority cao hơn Ready, task sẽ bị trễ, chu kỳ vẫn tính dựa trên xLastWakeTime
-> Muốn timing cực chính xác → cần task priority cao hoặc dùng hardware timer + ISR

**3. UART polling chiếm CPU**
- Khi TXE chưa xong → cả task đứng chờ → không làm gì khác
  
**4. Stack của Task cực quan trọng**
- Task_LED stack = 128 words
- Task_UART stack = 256 words
- Nếu stack nhỏ quá → crash không báo lỗi !
  
**5. Priority task = 1 giống nhau → round robin**
- Nếu có 1 task delay ít hơn hoặc chạy nhiều hơn → có thể gây starvation
  
   Starvation nghĩa là 1 task không bao giờ có cơ hội chạy
  
- Starvation xảy ra khi task “chiếm CPU quá nhiều” hoặc “luôn ready” → các task khác không được chạy
- Nên : Delay hợp lý , Task yield khi cần , Scheduler preemption, tick ngắn, Chú ý priority và priority inheritance

**Task gọi taskYIELD() :**

- Scheduler kiểm tra: Có task nào priority bằng hoặc cao hơn đang ready không?
- Nếu có → switch sang task đó ngay
- Nếu không → task hiện tại tiếp tục chạy
- taskYIELD() không delay, không block task, task vẫn ready ngay sau yield nếu scheduler quay lại
  
  **Scheduler preemption**
  
- Task A: priority 2, đang chạy
- Task B: priority 3 (cao hơn), vừa ready
- Nếu preemption bật → RTOS sẽ ngay lập tức cắt Task A, chạy Task B (không cần chờ đến delay)
- Nếu preemption tắt → Task A tiếp tục chạy, Task B phải chờ
-> Hầu hết FreeRTOS bật preemption mặc định
  
  **Tick ngắn** : ```c #define configTICK_RATE_HZ 1000  // tick = 1ms```
  
- Tick ngắn (1ms) → task delay 5ms gần như chính xác
- Tick dài (10ms) → task delay 5ms → thực tế delay = 10ms, task khác bị chậm → project real-time bị ảnh hưởng

 **Priority inversion** xảy ra khi task có priority cao bị block bởi task priority thấp vì task thấp đang giữ resource chung (ví dụ mutex)
- Priority Inheritance giải pháp : Khi một task cao priority bị block vì task thấp priority đang giữ mutex, task thấp sẽ tạm thời được “thừa hưởng” priority của      task cao
- Điều này giúp task thấp hoàn thành nhanh công việc và trả mutex, task cao sẽ chạy tiếp sớm hơn


## Video

https://drive.google.com/file/d/17KhQVlHO0ImcQLAzZ_8wkNhY3-68eOni/view?usp=drive_link

