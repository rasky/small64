#include <libdragon.h>

extern void demo(void);

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    // Simulate running in a context with no interrupts
    disable_interrupts();

    // Run the demo
    demo();
}
