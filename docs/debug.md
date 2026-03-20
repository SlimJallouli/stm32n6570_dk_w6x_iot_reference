# Build, Debug, and Flash

This guide explains the recommended STM32CubeIDE and STM32CubeMX workflow for building, debugging, and flashing firmware on STM32N6570-DK.

## Build in STM32CubeIDE

1. Import both projects:
   - `FSBL`
   - `Appli`
2. Build `Appli` for your current flow.
3. Build `FSBL` in:
   - Debug mode for debug sessions
   - Release mode for flashing and normal boot

## Debug in STM32CubeIDE

1. Build **FSBL** in **Debug** mode.
2. Set STM32N6570-DK to **Dev mode**.
3. Launch the provided debug configuration.
4. Let the debugger load:
   - `FSBL` (debug image)
   - `Appli` (RAM load)
5. Use hardware breakpoints.

## FSBL Debug and Release Boot Copy Behavior

In the FSBL path, `BOOT_Application()` in `Middlewares/ST/STM32_ExtMem_Manager/boot/stm32_boot_lrun.c` uses:

```c
#if !defined(_DEBUG_)
    retr = CopyApplication();
#endif
```

Meaning:

- Debug build: copy is skipped, and debugger-loaded RAM image is used.
- Release build: FSBL copies application image from external memory to RAM before jumping.

## Regenerating the Project with STM32CubeMX

### Before Opening the `.ioc`

Install these packs in STM32CubeMX before opening the project `.ioc`:

- ARM.CMSIS-FreeRTOS 11.2.0  
  https://www.keil.com/pack/ARM.CMSIS-FreeRTOS.11.2.0.pack
- ARM.mbedTLS 3.1.1  
  https://www.keil.com/pack/ARM.mbedTLS.3.1.1.pack
- AWS backoffAlgorithm 4.2.0  
  https://freertos-cmsis-packs.s3.us-west-2.amazonaws.com/AWS.backoffAlgorithm.4.2.0.pack
- AWS coreJSON 4.2.0  
  https://freertos-cmsis-packs.s3.us-west-2.amazonaws.com/AWS.coreJSON.4.2.0.pack
- AWS coreMQTT 5.1.0  
  https://freertos-cmsis-packs.s3.us-west-2.amazonaws.com/AWS.coreMQTT.5.1.0.pack
- AWS coreMQTT_Agent 5.1.0  
  https://freertos-cmsis-packs.s3.us-west-2.amazonaws.com/AWS.coreMQTT_Agent.5.1.0.pack
- lwIP 2.3.0  
  https://www.keil.com/pack/lwIP.lwIP.2.3.0.pack
- X-CUBE-ST67W61

When opening the `.ioc`, STM32CubeMX may request additional packs. Accept and install them.

### After Regeneration

- Re-check `stm32_boot_lrun.c` and re-apply the `_DEBUG_` guard if it was overwritten.
- Run `update.sh`.

## Flash Firmware

1. Build **FSBL** in **Release** mode.
2. Set STM32N6570-DK to **Dev mode**.
3. Power-cycle the board.
4. Run one of:
   - `flash.sh`
   - `flash.ps1`
5. Set the board to **Flash boot mode**.
6. Power-cycle again.
