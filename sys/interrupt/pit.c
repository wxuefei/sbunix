#include <sbunix/sbunix.h>
#include <sbunix/interrupt/pit.h>
#include <sbunix/interrupt/idt.h>
#include <sbunix/interrupt/pic8259.h>
#include <sbunix/console.h>
#include <sbunix/sched.h>

/* Programmable Interrupt Timer */
struct timespec unix_time;         /* real (UNIX) time */
volatile uint64_t system_time = 0; /* seconds since boot */
static uint32_t timer_ticks   = 0; /* current count of timer IRQs */
static uint32_t timer_hz      = 0; /* timer frequency */

/**
 * Timer interrupt handler
 */
void ISR_HANDLER(32) {
    timer_ticks++;
    if(timer_ticks == timer_hz) {
        timer_ticks = 1;
        system_time++;
        unix_time.tv_sec++;
        /* Print Seconds since boot in lower right corner of the console */
        writec_time(system_time);
    }
    /* Acknowledge interrupt */
    PIC_sendEOI(32);

    /* Timeslicing */
    if(curr_task->type & TASK_KERN && --curr_task->timeslice <= 0) {
        schedule();
    }

    /* TODO: wake up sleeping tasks */
}

/**
 * Blocking sleep function
 *
 * @seconds: seconds to block
 */
void timer_sleep(int seconds) {
    uint64_t wake_time = system_time + seconds;

    while(system_time < wake_time);
}

/**
 * Initialize the PIT to generate interrupts at the specified interval
 * @hz: The frequency in hertz to generate timer interrupts
 */
void pit_set_freq(unsigned int hz) {
    uint16_t reload_val;
    if(hz <= 18) {
        reload_val = 0; /* 0 reload value for PIT is ~18 HZ */
        timer_hz = 18;
    } else {
        reload_val = (uint16_t)(1193182/hz);
        timer_hz = hz;
    }
    timer_hz = reload_val? hz : 18;
    timer_ticks = 0;
    cli();
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)reload_val);        /* low byte of reload */
    outb(0x40, (uint8_t)(reload_val >> 8)); /* high byte of reload */
    sti();
}

/**
 * Read value from the RTC register
 * @reg: RTC register number to read
 */
static inline uint8_t read_rtc(uint8_t reg) {
    outb(RTC_PORT_REG, reg);
    return inb(RTC_PORT_DATA);
}

/**
 * Return the update in progress flag form the RTC
 */
static inline uint8_t read_rtc_update(void) {
    return read_rtc(RTC_REG_A) & (uint8_t)0x80;
}

/**
 * Return unix time stamp of current RTC
 */
unsigned long read_rtc_time(void) {
    unsigned int sec, min, hour, day, month, year;

    /* wait an update to the RTC.
     * NOTE: this will wait up to 1 second
     */
    while(!read_rtc_update());
    while(read_rtc_update());

    sec   = read_rtc(RTC_SECONDS);
    min   = read_rtc(RTC_MINUTES);
    hour  = read_rtc(RTC_HOURS);
    day   = read_rtc(RTC_DAY);
    month = read_rtc(RTC_MONTH);
    year  = read_rtc(RTC_MONTH);
    if(!(read_rtc(RTC_REG_B)&RTC_BINARY)) {
        /* translate from BCD to binary */
        sec   = BCD_TO_BIN(sec);
        min   = BCD_TO_BIN(min);
        hour  = BCD_TO_BIN(hour);
        day   = BCD_TO_BIN(day);
        month = BCD_TO_BIN(month);
        year  = BCD_TO_BIN(year);
    }
    year += 2011;
    /* translate RTC values to unix time */
//    printk("year:%u, month:%u, day:%u, hour:%u, min:%u, sec:%u\n", year, month,
//           day, hour, min, sec);
    return mktime(year, month, day, hour, min, sec);
}

/**
 * Read the current time from the RTC and initialize the unix time
 */
void init_unix_time(void) {
    unix_time.tv_sec  = read_rtc_time();
    unix_time.tv_nsec = 0;
}
