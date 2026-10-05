# Thread-Metric RTOS Test Suite

Thread-Metric measures RTOS primitive throughput by counting operations completed
in a fixed time interval (default 30 seconds). Results enable direct comparison
across different RTOS implementations on the same hardware.

Originally from Microsoft's [ThreadX](https://github.com/eclipse-threadx/threadx)
repository, released under the MIT license.

## Tests

| Test | File | What it measures |
|------|------|--------------------|
| Basic Processing | `src/basic_processing.c` | Single-thread throughput (baseline for scaling results) |
| Cooperative Scheduling | `src/cooperative_scheduling.c` | 5 equal-priority threads doing round-robin relinquish |
| Preemptive Scheduling | `src/preemptive_scheduling.c` | 5 threads at different priorities doing resume/suspend chains |
| Interrupt Processing | `src/interrupt_processing.c` | Software trap -> ISR posts semaphore -> thread picks it up |
| Interrupt Preemption | `src/interrupt_preemption_processing.c` | Software trap -> ISR resumes higher-priority thread |
| Message Processing | `src/message_processing.c` | Single thread send/receive of 4-unsigned-long queue messages |
| Synchronization | `src/synchronization_processing.c` | Single thread semaphore get/put cycle |
| Memory Allocation | `src/memory_allocation.c` | Single thread 128-byte block allocate/deallocate cycle |

## Architecture

```
configs/
  Kconfig               # Build configuration schema (RTOS, target, options)
  *_defconfig            # Pre-made configurations for each RTOS+target

mk/
  build.mk               # Toolchain, flags, build directory, verbosity

include/
  tm_api.h               # RTOS-neutral API: 14 functions + tm_cause_interrupt +
                         #   tm_cause_interrupt_sync (see Porting Layer below)

src/
  *.c                    # One test per file, each defines tm_main()

ports/
  common/
    cortex-m/            # Shared Cortex-M3 bootstrap for QEMU mps2-an385
      startup.S          #   Reset handler, BSS/data init, semihosting setup
      vector_table.c     #   Default NVIC handlers (weak aliases)
      mps2_an385.ld      #   Linker script (4 MB FLASH + 4 MB SRAM)
  threadx/               # ThreadX porting layer
    tm_port.c            #   Porting layer (14 functions + cause-interrupt pair)
    main.c               #   Entry point
    posix-host/          #   POSIX simulator config (Linux/macOS)
    cortex-m/            #   Cortex-M3 QEMU support (SVC dispatch, SysTick)
  freertos/              # FreeRTOS porting layer
    tm_port.c            #   Porting layer (14 functions + cause-interrupt pair)
    main.c               #   Entry point
    posix-host/          #   POSIX simulator config (FreeRTOSConfig.h)
    cortex-m/            #   Cortex-M3 QEMU support (NVIC IRQ dispatch)

scripts/
  qemu-run.sh            # QEMU runner with semihosting + timeout
```

Two layers, one boundary: tests call the API in `tm_api.h`, the porting layer
maps those calls to real RTOS primitives. Each binary links exactly one test
source file.

## Supported RTOS Ports

| RTOS | POSIX Host | Cortex-M QEMU | Interrupt Tests |
|------|-----------|---------------|-----------------|
| ThreadX | yes | yes (mps2-an385) | yes |
| FreeRTOS | yes | yes (mps2-an385) | yes |

Both ports have been tested with all 8 tests on both targets (`make check`).

## Porting Layer

Implement the 14 functions declared in `tm_api.h` plus the two
interrupt-cause primitives:

- `tm_cause_interrupt()` — must traverse the RTOS's real interrupt path
  (SVC, NVIC pend, semaphore-to-ISR-thread, etc.) so the handler runs
  with full context save/restore and any handler-triggered higher-
  priority resume causes preemption. Used by
  `interrupt_preemption_processing.c`.
- `tm_cause_interrupt_sync()` — must invoke `tm_interrupt_handler()`
  in-line on the caller's stack, with no trap and no scheduler round-
  trip. Used by `interrupt_processing.c` to measure the cost of an ISR
  body without the preemption-path overhead.

See `ports/threadx/tm_port.c` or `ports/freertos/tm_port.c` for references.

Requirements for fair benchmarking:
- Functions must be real calls, not macros
- `tm_thread_sleep` takes seconds; each port maps that to its native tick rate
- Queue messages are `4 * sizeof(unsigned long)` bytes (16 on ILP32, 32 on LP64)
- Memory pool blocks are 128 bytes
- No cache locking of test or RTOS code regions
- Use 30-second measurement intervals

## Building

The build system uses [Kconfiglib](https://github.com/sysprog21/Kconfiglib)
for RTOS and target selection.
Kconfiglib is downloaded automatically on first use.

### Quick start

Configure once, then build:
```shell
make defconfig          # Apply default config (ThreadX + POSIX host)
make                    # Build all test binaries
```

Or configure and build in a single command:
```shell
make defconfig all
```

### Selecting an RTOS and target

Named defconfig files provide pre-made configurations:
```shell
make threadx_posix_defconfig        # ThreadX + POSIX host
make threadx_cortex_m_defconfig     # ThreadX + Cortex-M3 QEMU
make freertos_posix_defconfig       # FreeRTOS + POSIX host
make freertos_cortex_m_defconfig    # FreeRTOS + Cortex-M3 QEMU
```

For interactive configuration with a menu interface:
```shell
make config
```

The RTOS kernel is cloned automatically on first build (ThreadX from
eclipse-threadx/threadx, FreeRTOS from FreeRTOS/FreeRTOS-Kernel).

### Common targets

```shell
make                    # Build all test binaries
make tm_basic_processing # Build a single test
make check              # Build with 3 s intervals + 1 cycle, run all tests
make clean              # Remove binaries and build directory
make distclean          # Also remove .config, cloned RTOS trees, Kconfiglib
make help               # Show all available targets and overrides
```

### POSIX host

ThreadX POSIX port uses `pthread_setschedparam`, which requires elevated
privileges on Linux:
```shell
sudo ./tm_basic_processing
```

FreeRTOS POSIX port runs without `sudo`.

### Cortex-M QEMU

Requires `arm-none-eabi-gcc` and `qemu-system-arm`. The build system
auto-sets `CROSS_COMPILE=arm-none-eabi-` and validates the cross-compiler
exists before building. Override with an explicit path if needed:
```shell
make CROSS_COMPILE=/opt/toolchain/arm-none-eabi/bin/arm-none-eabi-
```

Run under QEMU (auto-terminates via semihosting after one report):
```shell
make run
```

Or run the QEMU script directly:
```shell
scripts/qemu-run.sh tm_basic_processing -semihosting-config enable=on,target=native
```

### Build options

Kconfig options can be set via `make config` (interactive) or by editing
`.config` directly:

| Option | Default | Effect |
|--------|---------|--------|
| `CONFIG_TEST_DURATION` | 30 | Reporting interval in seconds |
| `CONFIG_TEST_CYCLES` | 0 | Reports before exit (0 = infinite) |
| `CONFIG_OPTIMIZE_SIZE` | n | Use `-Os` instead of `-O2` |
| `CONFIG_DEBUG_SYMBOLS` | n | Add `-g` |
| `CONFIG_SANITIZERS` | n | Enable ASan/UBSan (POSIX host only) |

Command-line overrides still work for test parameters:
```shell
make TM_TEST_DURATION=5 TM_TEST_CYCLES=1
```

Verbose build output:
```shell
make V=1
```

## Adding a New RTOS Port

1. Create `ports/<rtos>/tm_port.c` implementing the 14 functions in
   `tm_api.h` plus the two cause-interrupt primitives
   (`tm_cause_interrupt`, `tm_cause_interrupt_sync`)
2. Create `ports/<rtos>/main.c` with RTOS-specific startup
3. For POSIX host: provide configuration headers in `ports/<rtos>/posix-host/`
4. For Cortex-M QEMU: provide RTOS config and ISR dispatch in
   `ports/<rtos>/cortex-m/`; the shared bootstrap in `ports/common/cortex-m/`
   handles reset, vector table, and linker script
5. Add a `CONFIG_RTOS_<NAME>` choice entry in `configs/Kconfig`
6. Add an `ifeq ($(CONFIG_RTOS_<NAME>),y)` block in the Makefile
7. Create `configs/<rtos>_posix_defconfig` and `configs/<rtos>_cortex_m_defconfig`
