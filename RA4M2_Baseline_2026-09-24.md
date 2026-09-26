# RA4M2 基线证据记录

日期：2026-09-24  
工程：`E:\RenesasProject\RA4M2_Blink`  
记录范围：阶段0基线冻结，以及不涉及硬件的当时工程 Rebuild。2026-09-25 后续实机进展见下文补录；原始基线数据不追改为当前值。

## 已确认

- 工程目标：`Target_1`。
- 配置目标：`R7FA4M2AD3CFL`，Keil Device：`R7FA4M2AD`。
- RASC/FSP：配置文件记录 FSP `6.6.0`，RTOS 为 `_none`，Board BSP 为 `custom`。
- Keil 工具链：Arm Compiler 6.24；本机命令行入口：`E:\STM\UV4\UV4.exe`。
- RASC 可执行文件：`D:\Renesas\eclipse\rascc.exe`；工程的 `rasc_version.txt` 仍记录版本为 `Unknown`。
- 2026-09-24 执行了明确的 Rebuild：
  `E:\STM\UV4\UV4.exe -r E:\RenesasProject\RA4M2_Blink\RA4M2_Blink.uvprojx -t Target_1 -o E:\RenesasProject\RA4M2_Blink\Target_1_build.log`
- Rebuild 结果：`0 Error(s), 0 Warning(s)`，耗时约 28 秒。
- 固件大小：Code=1808，RO-data=412，RW-data=0，ZI-data=1136；报告工具估算 Flash 约 2.2 KB、RAM 约 1.1 KB。
- 已生成/更新产物：
  - `E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf`
  - `E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex`
  - `E:\RenesasProject\RA4M2_Blink\Listings\RA4M2_Blink.map`

## 阶段0哈希（SHA-256，2026-09-24历史快照）

```text
6A0A9C454AC3657EA5821F1E2FD9B96644C441A56BBECC73670DA7DAF15E0DF1  E:\RenesasProject\RA4M2_Blink\configuration.xml
CFBBBDF6C8EBB5A469A0331DA8489673A8EE606A15F68A6927238A354A93FBD1  E:\RenesasProject\RA4M2_Blink\RA4M2_Blink.uvprojx
96C37F546F9947F7EB59DC3046EB59D91742C9169F4128BBF285BAACD9FA66C7  E:\RenesasProject\RA4M2_Blink\src\hal_entry.c
1E198E81EC658FCF83D28EF127F7DE105759E16C9252D3D2712F62982BF01714  E:\RenesasProject\RA4M2_Blink\ra_gen\pin_data.c
97DC5446E7BD3CE702DAED9DAF5ACAFF6724F076A265974DC31C812C0068F170  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.build_log.htm
384DB3650E64CDE614B01D6413A89D09949AF2CDCB723B0F778155C82D65FD03  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
781120B376ED04A6C47A8B466C714B2615F26C39F67067C007C1B6FAD076B619  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
B83DE17E513B61B50CFC41D29CDBE79CCECCDC766B42193A28FD40A502C62175  E:\RenesasProject\RA4M2_Blink\Listings\RA4M2_Blink.map
51FD9B08AD5A727E5968466FD6DE5628C97D751389CA279A104AA0D91FD116C2  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 阶段0时仍未确认

- `RA4M2_Blink.uvprojx` 中仍存在 `InvalidFlash=1`；但用户随后提供的 Keil 界面证据显示实际已识别并使用 Flash Algorithm，需将该字段视为工程文件与当前 Keil 用户配置之间的待对齐项，不据此否定已完成的烧录证据。
- 在本机已检查的常见 Pack 目录（`E:\STM\ARM\PACK`、`C:\Keil_v5\ARM\PACK`、`D:\Keil_v5\ARM\PACK`）中未找到匹配 Pack 文件；用户已确认 Device Pack 已安装，安装位置尚未由本机文件检查定位。
- RASC 实际版本号仍未从工程记录中确认。
- DA16200 载板方向、供电、UART 电平、AT 参数和任何联网能力均未测试。
- 当前上层 `E:\` Git 工作树没有提交，`RenesasProject` 显示为未跟踪目录；未执行任何清理、还原或提交。

## 用户补充的实机证据

以下为用户于本次交接后的现场确认，属于用户提供的实机证据，本次未由 Codex 代替用户复测：

- RA4M2 开发板可稳定上电运行。
- CMSIS-DAP/DAPLink 通过 SWD 成功连接并烧录 RA4M2。
- Device Pack 为 Renesas `RA_DFP 6.5.1`，RA4M2 设备和 Flash Algorithm 可正常识别。
- 已配置 `RA4M2 512K Flash` 与 `RA4M2 Config Area`；算法 RAM 为 `0x20000000 / 0x7800`。
- Keil 显示 `Erase Done / Programming Done / Verify OK`。
- 烧录后 P103 对应用户 LED D3 按 500 ms 周期闪烁。

工程侧只读复核到的对应配置位于 `RA4M2_Blink.uvoptx:123`：包含 `RA4M2_512K.FLM`、`RA4M2_CONF.FLM`、`-FD20000000` 和 `-FC7800`。

## DA16200 单独验证结果（用户补充）

以下为 2026-09-25 用户提供的实机验证结果：

- DA16200 载板 3.3 V 独立供电正常，模块能够正常启动。
- J2 的 VCC、UART0_RXD、UART0_TXD、GND 已确认，UART0 Console 通信正常。
- 已升级为 `DA16200 FreeRTOS V3.3.0.0 UART1 ATCMD` 固件。
- 早期万用表追线曾将 GPIOA4 / UART1 TX 推定为 `J1-3`、GPIOA5 / UART1 RX 推定为 `J1-5`；这一推定已被 2026-09-25 的载板原理图与重新断电通断实测纠正，不能再据此接线。GND 推定为 `J1-9`。
- UART1 双向通信在 115200、8N1、无流控下正常。
- `AT+VER` 返回 `+VER:FRTOS-GEN01-01-ccd7e9513f-007000` 和 `OK`。

本节属于用户提供的 DA 单独验证证据；J1 信号映射以后续补录的原理图和重新通断实测为准。

## 2026-09-25 RA4M2 ↔ DA16200 最小 UART 联调补录

- 当前工程由 RASC/FSP 6.6.0 配置 SCI0：RA4M2 `P101/TX`、`P100/RX`，115200、8N1、无流控；用户代码仅验证 `AT+VER\r\n`，不包含 Wi-Fi、TCP、MQTT 或传感器功能。
- DA16200 载板原理图将 `R11` 连接 GPIOA4（UART1 TX）与 `J1-5/RXD_HOST`，将 `R12` 连接 GPIOA5（UART1 RX）与 `J1-3/TXD_HOST`；`TXD_HOST` / `RXD_HOST` 采用主控视角命名。用户断电通断实测 `R11 ↔ J1-5`、`R12 ↔ J1-3`，并在 DA 单独供电时测得 J1-5 约 3.3 V、J1-3 约 0.03 V。这里的 J1 映射有原理图与实物通断双重依据。
- 原先 `RA P101/TX → DA J1-5`、`DA J1-3 → RA P100/RX` 接反。用户断电后改为 `RA P101/TX → DA J1-3/RX`、`DA J1-5/TX → RA P100/RX`，并保持公共 GND。
- 2026-09-25 当前 Keil `Target_1` 构建日志为 `0 Error(s), 0 Warning(s)`；用户完成烧录。此构建不同于上方 2026-09-24 的阶段0闪灯基线，不能混用产物大小和哈希。
- Codex 通过 CMSIS-DAP/OpenOCD 短暂停住 RA、只读 RAM 后恢复运行；观察到 `g_at_attempt=2`、`g_at_result=1`、`g_uart_error=0`、`g_last_uart_event=0`、`g_tx_complete=1`，`g_version_line="+VER:FRTOS-GEN01-01-ccd7e9513f-007000"`。该次 OpenOCD 调试未复位、未烧录。
- 用户提供的 Keil Watch 截图显示 `g_at_result=1`、`g_uart_error=0`、`g_last_uart_event=0` 和有效版本行；用户随后报告另外两次断电重启均通过。按用户反馈共三次启动通过，其中仅首轮有本次截图证据；后两次未由 Codex 逐次读取调试器。
- 当前代码在成功后停止消费 RX 环形缓冲区，但接收回调继续写入；OpenOCD 一次快照中的 `g_rx_dropped=0x691` 因此不能用于否定已捕获的 `+VER`/`OK`，但连续 AT 通信前需要处理接收缓冲区生命周期。

本补录仅将最小 `AT+VER` 往返标为已构建、已烧录（用户完成）、已调试并在当前接线下实机通过；不将三次启动扩大为长期稳定性，也不声称已完成配网、云端、传感器或机器人整机验证。

## 下一步边界

RA4M2 ↔ DA16200 的最小 `AT+VER` 联调已按上述边界完成。后续若要进入持续 AT 通信，先处理成功后 RX 环形缓冲区不再消费的问题；Wi-Fi 配网、TCP/MQTT、历史数据重放和传感器属于尚未开始的新阶段，需另行确定范围与验收条件。

## 证据文件

- 构建命令日志：`E:\RenesasProject\RA4M2_Blink\Target_1_build.log`
- Keil 构建日志：`E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.build_log.htm`
- 工程报告：`E:\RenesasProject\RA4M2_DA16200巡线机器人替代方案交接报告_2026-09-24.md`
