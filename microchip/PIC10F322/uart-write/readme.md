# UART write with PIC10F322

## Device

- Small device with 8 pins (2 of them are NC)
- 512 words of flash memory.
- 64 bytes of memory.
- (maximum) 16MHz internal clock
- 8 level hardware stack

[datasheet](../PIC10F322.pdf)

## Lessons learned

### Every bit matters

Each project needs a set of configuration bits specified in the code. There are a couple of these, with new concepts to learn. Conveniently, MPXLab IDE contains a feature to generate the configuration bits.

With eager to make some progress, I just accepted the default configuration bits, without understanding the meaning of each settings.

As it turned out, one of the settings controls if the microchip is run by an external clock or by its internal clock. The default set is EC (external clock). It took me quite some time to figure out why the microchip did not execute my code.

Compared to high level programming, it feels increasingly important to understand every bits of detail when working with hardware. Even for prototyping.

### internal clock speed is configurable

The initial prototype contained a `for` loop to set the bits of the UART communication, and used `__delay_us()` to wait between them.
For `__delay_us` to work, one needs to define `_XTAL_FREQ` so that appropriate number of `NOP`s would be spent for waiting.
I set `#define _XTAL_FREQ 16000000L` assuming I was working with 16MHz internal clock.

Interestingly, the delays took roughy twice as much time than I expected.

It turned out, the internal clock speed is configurable. The 16MHz clock speed is only the maximum. By default, 8MHz internal clock is used. So I set `#define _XTAL_FREQ 8000000L`. That removed the problem of bits taking twice as much time. It would have been also an option to configure the 16MHz internal clock.

Despite that, for the inner bits of the for cycle, I still experienced significant deviation (extra 16us) from the expected signal length (104us). See next lesson.

### instructions are slow

Instruction cycle and clock cycle are different concepts. For PIC10F322, one instruction cycle takes 4 clock cycle.

I am using 8MHz clock cycle, which yields 2MHz instruction cycle => one instruction takes 0.5us. Execution time from instructions can quickly add up.

A simple command may read a value from memory to a register, execute an operation inside the register, and writes the result back to memory. With 3 instructions, this takes 1.5us. Bits in an UART communication with 9600 baud rate take 104us. Such command takes more than 1% of the delay between the UART bits.

It haven't felt much back then, but managing a `for` cycle: incrementing the index, branching on comparison, calculating the next bit, and writing the result to gpio took quite a lot of time on the top of `__delay_us()`. Considering 8 bit UART data, with the measured extra 16us with the inner bits (6*16=108), it gets hard to reason about correctness from an UART reader perspective. Reading happens in the middle of an interval, so this might have worked, but it felt safer to adjust the delays with that in consideration.

Empirical as it is, the approach got the prototype working. Then I decided to make another attempt with a different approach using a software timer.

### Bread boards and loose connections

There is a sanity check at the beginning of the flashing (asserting some kind of device information from the IC). I ran into the sanity check failing. As it turned out, as I kept pushing and popping the IC into the bread board, the connections got loose, and due to that, the data transfer was unreliable. In the end, I needed to manually press down the IC to get a firm connection.

Similarly, after a successful flash, there were a lot of errors happening in the UART communication due to the unreliable connections.

That's when I learned that bread boards are helpful in prototyping, but I need something more reliable even a projet of this size. Ultimately, I soldered the flashing circuit and the test circuit to perfboards.

### ide marks register symbols as undefined

By recommendation, users need to include a generic `#include <xc.h>` header, instead of the PIC specific headers. Using different compile parameters, the compiler will preprocess that file to specialize it for the underlying IC. For some reason, those parameters do not make into the MPXLab ide, so the static analyzer does not recognize those fields. The code compiles fine, but the errors from a static analyzers were a bit surprising at first.

## The code

The final version of the code relies on software timers interrupts.

The way timer2 works is, it increments the timer2 value from zero at each instruction cycle (with the default 1:1 pre-scaler). Once a specific value is reached, a software interrupt is generated (with the default 1:1 post-scaler).

Conveniently, changing the pre-scaler to 1:2, one increment of the timer value takes 2*instruction cycle, which was 1us using the 8Mhz clock cycle (=> 2MHz instruction cycle => 0.5us instruction time).

That means, I could simply set the comparison value to 104 to align to the 9600 baud rate (1/9600s  = 0.000104166s = 104.166us)

For PIC10F322, there is only one element in the interrupt vector. This means, a single function should handle all kind of interrupts. The way this works is, when a specific interrupt happens, the respective interrupt flag is set. The code can check all interrupt flags to find out which event(s) generated the interrupt.

In my project, I only used timer2. TMR2IF is the only flag I needed to manage in the interrupt handler.

TMR2IF is considered a peripherial interrupt. So it was not enough to enable global interrupts (GIE). I needed to enable the peripherial interrupts too (PEIE). That took me by surprise.

I decided to implement a blocking api: stopping the execution of `main` until the UART communication finishes.

Alternatively, I could have filled data buffers, and turn UART communication into an asynchronous call. However the hardware does not have too much memory (512 words of flash and 64 bytes of memory). So using an asynchronous API would have had significant buffering limitations. Also, it would have introduced significant code complexity, so I avoided it for now. Single threaded asynchronity is an interesting topic on it's own. It is worth a dedicated project for experimentation.

Blocking is implemented by using an `in_use` boolean flag. When `main` requests an UART communication
- `in_use` flag is set to true
- the data is passed to the `UART` module
- software timer is started.

Then the code polls `in_use` infinitely, until it is set to false, hence the block. When all the data is consumed, `in_use` is set to false, allowing `main` to continue.

It felt like a good idea to add a small abstraction over the data consumption, so I created a `Iterator` object. Users (in my case, the interrupt handler) can call `next` on the interator to fetch the next bit. The iterator is a state machine internally, which knows if we need to emit the start bit, data bits or the stop bit.

It might have been beneficial to add additional layer of abstractions that I skipped for now: `next` could have worked with a `FrameIterator` responsible for assembling the frame around a `DataIterator`. For clean code perspective, this might have been a sound abstraction.

Using timers for the delays instead of busy loops introduced quite some complexity. However, in some aspects, it is easier to reason for correctness. For example, I did not need to adjust the size of the busy loops based on the instructions needed to execute the next bit, as I explained in the lessons learned section.

The way this POC works is:
1) Interrupt event
2) Next bit is calculated
3) Next bit is emitted

Although it is easier to reason about the timing of the trigger point, calculating the next bit is still a complicated task. Not even uniform amongst the communication bits: for example calculating the start bit is faster than calculating a data bit. This did not cause trouble for me, I managed to get reliable reads from the device.

I was considering an improvement, though. I noticed the only time-sensitive parts are the generation of the interrupt, and the bit emission. Data calculation could be executed anytime within the delay. As a refactor, I could have shifted the calculation of the next bit to a previous iteration. It is the interrupt handler that calculates the next bits, but only after the time sensitive bit emission happened:

1) Interrupt event
2) Emit previously calculated next bit
3) Calculate and save the next bit for future iteration

This variant would have tied the bit emission close to the beginning of the interrupt handler, resulting in minimal latency from the interrupt event. This would have added additional complexity to the code, bit this architecture would have been the cleanest to reason about timing.

If the device had significant memory, it would have been an option to pre-compile the data. For example fill the bits into a pre-allocated large array. With a device of such a small size, this was not really suitable. However, a nice property would have been that the data calculation would have been taken out of the interrupt handler, making the interrupt handler more lightweight.

The final result took 22% of the available flash space, and 16% of the memory space using free compiler.
