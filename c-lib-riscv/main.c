#include <stddef.h>
#include <stdint.h>

// common assembly instructions

#define wfi asm volatile("wfi")

#define mret                                                                   \
  asm volatile("mret");                                                        \
  __builtin_unreachable()

#define hotloop                                                                \
  while (1) {                                                                  \
  }

// csr instructions

#define csrr(csr, rd) asm volatile("csrr %0, " #csr : "=r"(rd))
#define csrw(csr, rs1) asm volatile("csrw " #csr ", %0" ::"r"(rs1))
#define csrs(csr, rs1) asm volatile("csrs " #csr ", %0" ::"r"(rs1))
#define csrc(csr, rs1) asm volatile("csrc " #csr ", %0" ::"r"(rs1))

// control and status registers

// mstatus register

#define MSTATUS_MIE (1 << 3)
#define MSTATUS_MPIE (1 << 7)
#define MSTATUS_MPP (3 << 11)
#define MSTATUS_MPP_M (3 << 11)
#define MSTATUS_MPP_S (1 << 11)

// mie register

#define MIE_MSIE (1 << 3)
#define MIE_MTIE (1 << 7)
#define MIE_MEIE (1 << 11)

// xcause register

enum XCauseException {
  EXC_INSTRUCTION_ACCESS_FAULT = 1,
  EXC_ILLEGAL_INSTRUCTION = 2,
  EXC_LOAD_ACCESS_FAULT = 5,
  EXC_STORE_AMO_ACCESS_FAULT = 7,
  EXC_INSTRUCTION_PAGE_FAULT = 12,
  EXC_LOAD_PAGE_FAULT = 13,
  EXC_STORE_AMO_PAGE_FAULT = 15,
};

const char *exception_names[] = {
    [EXC_INSTRUCTION_ACCESS_FAULT] = "Instruction access fault",
    [EXC_ILLEGAL_INSTRUCTION] = "Illegal instruction",
    [EXC_LOAD_ACCESS_FAULT] = "Load access fault",
    [EXC_STORE_AMO_ACCESS_FAULT] = "Store/AMO access fault",
    [EXC_INSTRUCTION_PAGE_FAULT] = "Instruction page fault",
    [EXC_LOAD_PAGE_FAULT] = "Load page fault",
    [EXC_STORE_AMO_PAGE_FAULT] = "Store/AMO page fault",
};

enum XCauseInterrupt {
  IRQ_MACHINE_SOFTWARE_INTERRUPT = 3,
  IRQ_MACHINE_TIMER_INTERRUPT = 7,
  IRQ_MACHINE_EXTERNAL_INTERRUPT = 11,
  IRQ_COUNTER_OVERFLOW_INTERRUPT = 13,
};

const char *irq_names[] = {
    [IRQ_MACHINE_SOFTWARE_INTERRUPT] = "Machine software interrupt",
    [IRQ_MACHINE_TIMER_INTERRUPT] = "Machine timer interrupt",
    [IRQ_MACHINE_EXTERNAL_INTERRUPT] = "Machine external interrupt",
    [IRQ_COUNTER_OVERFLOW_INTERRUPT] = "Counter overflow interrupt",
};

typedef struct {
  uint32_t code : 31;
  uint32_t is_interrupt : 1;
} __attribute__((packed)) XCause;

XCause newMCause() {
  uint32_t cause;
  asm volatile("csrr %0, mcause" : "=r"(cause));
  return *(XCause *)&cause;
}

// plic

enum PLIC_SRC {
  PLIC_SRC_UART = 0xA,
};

typedef struct {
  uint32_t *volatile priority;
  uint32_t *volatile enable;
  uint32_t *volatile threshold;
  uint32_t *volatile claim;
  uint32_t *volatile complete;
} PlicDriver;

PlicDriver newPlicDriver(size_t base) {
  return (PlicDriver){
      .priority = (uint32_t *)(base + 0x0),
      .enable = (uint32_t *)(base + 0x2000),
      .threshold = (uint32_t *)(base + 0x200000),
      .claim = (uint32_t *)(base + 0x200004),
      .complete = (uint32_t *)(base + 0x200004),

  };
}

#define PLIC_DECLARE(base)                                                     \
  ((PlicDriver){.priority = (uint32_t *)(base + 0x0),                          \
                .enable = (uint32_t *)(base + 0x2000),                         \
                .threshold = (uint32_t *)(base + 0x200000),                    \
                .claim = (uint32_t *)(base + 0x200004),                        \
                .complete = (uint32_t *)(base + 0x200004)})

size_t plic_context(size_t mode) {
  int hart;
  asm volatile("csrr %0, mhartid" : "=r"(hart));
  return hart << 1 | mode;
}

size_t plic_priority_index(size_t src) { return (src * 0x4) >> 2; }

void plic_priority_set(PlicDriver *p, size_t src, size_t prio) {
  p->priority[plic_priority_index(src)] = prio;
}

size_t plic_enable_index(size_t ctx, size_t src) {
  return (ctx * 0x80 + (src >> 5)) >> 2;
}

void plic_enable_set(PlicDriver *p, size_t ctx, size_t src) {
  p->enable[plic_enable_index(ctx, src)] |= 1 << (src & 31);
}

size_t plic_threshold_index(size_t ctx) { return (ctx * 0x1000) >> 2; }

void plic_threshold_set(PlicDriver *p, size_t ctx, size_t th) {
  p->threshold[plic_threshold_index(ctx)] = th;
}

size_t plic_claim_index(size_t ctx) { return (ctx * 0x1000) >> 2; }

size_t plic_claim(PlicDriver *p, size_t ctx) {
  return p->claim[plic_claim_index(ctx)];
}

size_t plic_complete_index(size_t ctx) { return (ctx * 0x1000) >> 2; }

void plic_complete(PlicDriver *p, size_t ctx, size_t src) {
  p->complete[plic_complete_index(ctx)] = src;
}

// uart

typedef struct {
  uint8_t *ports;
} UartDriver;

UartDriver newUartDriver(size_t base) {
  return (UartDriver){.ports = (uint8_t *)base};
}

#define UART_DECLARE(base) ((UartDriver){.ports = (uint8_t *)base})

void uart_rtx_write(UartDriver *uart, char c) {
  while ((uart->ports[0x5] & 0x20) == 0)
    ;
  if (c == '\n')
    uart->ports[0x0] = '\r';
  uart->ports[0x0] = c;
}

char uart_rtx_read(UartDriver *uart) {
  while ((uart->ports[0x5] & 0x1) == 0)
    ;
  char c = uart->ports[0x0];
  if (c == '\r')
    c = '\n';
  return c;
}

void uart_rtx_flush(UartDriver *uart) {
  while ((uart->ports[0x5] & 0x40) == 0)
    ;
}

void uart_fifo_init(UartDriver *uart) { uart->ports[0x2] |= (1 | 3 << 1); }

uint8_t uart_fifo_status(UartDriver *uart) { return uart->ports[0x2] >> 6 & 3; }

void uart_irq_enable_set(UartDriver *uart, uint8_t flags) {
  uart->ports[0x1] |= (flags & 0xf);
}

void uart_irq_enable_clear(UartDriver *uart, uint8_t flags) {
  uart->ports[0x1] &= ~(flags & 0xf);
}

uint8_t uart_irq_is_pending(UartDriver *uart) {
  return (uart->ports[0x2] & 1) == 0;
}

// print functions

typedef void Write(void *, char);

typedef struct {
  void *impl;
  Write *write;
} Writer;

void fprint(Writer *w, const char *s) {
  while (*s)
    w->write(w->impl, *s++);
}

// string functions

const char *hexchars = "0123456789abcdef";

char *itoa(size_t base, size_t num, char *buf) {
  char *p = buf + 35;

  *--p = '\0';

  do {
    *--p = hexchars[num % base];
    num /= base;
  } while (num);

  if (base == 2)
    *--p = 'b';
  if (base == 8)
    *--p = 'o';
  if (base == 10)
    *--p = 'd';
  if (base == 16)
    *--p = 'x';

  *--p = '0';

  return p;
}

// ====================================
// main program
// ====================================

void irq_handler();

int main() {
  // load the trap as first thing, to catch any exception.
  // the setup uart in order to print debug messages.
  csrw(mtvec, &irq_handler);

  UartDriver uart = newUartDriver(0x10000000);
  Writer w = (Writer){.impl = &uart, .write = (Write *)uart_rtx_write};
  uart_fifo_init(&uart);

  // start the real initialization
  fprint(&w, "init\n");

  // enable interrupts
  csrs(mstatus, MSTATUS_MPP_M | MSTATUS_MPIE | MSTATUS_MIE);
  csrs(mie, MIE_MSIE | MIE_MEIE);
  fprint(&w, "interrupts enabled\n");

  // enable uart interrupts on PLIC
  PlicDriver p = newPlicDriver(0x0c000000);
  int ctx = plic_context(0);
  plic_priority_set(&p, PLIC_SRC_UART, 1);
  plic_enable_set(&p, ctx, PLIC_SRC_UART);
  plic_threshold_set(&p, ctx, 0);
  fprint(&w, "PLIC configured for UART\n");

  // enable uart interrupts
  uart_irq_enable_set(&uart, 1);
  fprint(&w, "UART interrupts enabled\n");

  fprint(&w, "waiting for interrupts\n");
  while (1)
    uart_rtx_write(&uart, uart_rtx_read(&uart));
}

void irq_handler() {
  UartDriver uart = newUartDriver(0x10000000);
  Writer w = (Writer){.impl = &uart, .write = (Write *)uart_rtx_write};
  char buf[35];

  fprint(&w, "irq: ");

  XCause cause = newMCause();
  fprint(&w, itoa(2, cause.is_interrupt, buf));
  fprint(&w, " : code: ");
  fprint(&w, itoa(16, cause.code, buf));
  fprint(&w, ": ");

  if (!cause.is_interrupt) {
    fprint(&w, "exception: ");
    fprint(&w, exception_names[cause.code]);
    hotloop;
  }

  fprint(&w, "interrupt: ");
  fprint(&w, irq_names[cause.code]);
  fprint(&w, ": ");

  if (cause.code == 11) {
    PlicDriver p = newPlicDriver(0x0c000000);
    int ctx = plic_context(0);
    size_t src = plic_claim(&p, ctx);
    fprint(&w, "PLIC source: ");
    fprint(&w, itoa(16, src, buf));
    plic_complete(&p, ctx, src);
    fprint(&w, "\n");
    hotloop;
  }

  fprint(&w, "\n");
  hotloop;
}
