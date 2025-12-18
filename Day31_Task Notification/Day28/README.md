```
   +-------------+
   |  Tera Term  |
   |     PC      |
   +------+------+
         |
         | UART RX
         v
+--------------------+
| USART1 RX ISR      |
| (RXNE interrupt)   |
+----------+---------+
           |
           | char
           v
+--------------------+
| xQueueRxChar       |
| (char queue)       |
+----------+---------+
           |
           v
+------------------------------+
| Task_UART_Receiver           |
| - Echo                       |
| - Parse ON/OFF/TOGGLE        |
+----------+-------------------+
           |
           | TaskNotify(cmd)
           v
+------------------------------+
| Task_LED_Control              |
| - LED ON/OFF/TOGGLE           |
| - Prepare log message         |
+----------+-------------------+
           |
           | char* message
           v
+------------------------------+
| xQueueTxMsg                  |
| (char* queue)                |
+----------+-------------------+
           |
           v
+------------------------------+
| Task_UART_TX                 |
| - Send string via IT         |
+----------+-------------------+
           |
           | char
           v
+------------------------------+
| xQueueTxChar                 |
| (char queue)                 |
+----------+-------------------+
           |
           v
+------------------------------+
| USART1 TX ISR                |
| (TXE interrupt)              |
+----------+-------------------+
           |
           | UART TX
           v
    +-------------+
    |  Tera Term  |
    +-------------+

   [ Software Timer ]
           |
           v
xQueueTxMsg --> UART TX

```
