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

## 2026-09-26 持续 AT、扫描与入网补录

- 用户明确确认阶段 1 通过。当前用户代码已经实现 UART RX 环形缓冲区持续消费、按行解析、同步/异步响应区分和诊断计数；先前最小版本在成功后不再消费 RX 的限制已在源码层处理。
- 用户提供的 Keil Watch 证据显示基础同步、版本和 STA 模式查询成功，国家码查询最终为 `+WFCC:CN`，DPM 查询为 `+DPM:0`。
- AP 扫描曾成功返回，Watch 先后显示 4 个和 5 个 AP；目标热点至少一次被识别，`g_target_ap_found=1`。扫描数量仅代表当时无线环境。
- 热点凭据当前保存在本机 `RA4M2_Blink\src\config\wifi_credentials.h`，并由仓库根目录 `.gitignore` 排除；本文不保存真实密码。含凭据的 AXF/HEX 也不得公开上传。
- 首次入网命令被接受：`g_join_command_result=1`；异步结果为 `g_join_result=2`、`g_join_line="+WFJAP:0,TIMEOUT"`，因此 Wi-Fi 入网尚未通过。
- 后续重新启动时，Watch 显示 `g_at_attempt=0x22`、`g_at_sync_result=3`、`g_at_result=3`、`g_uart_error=1`，程序被暂停在重试延时函数中。该证据表明启动阶段反复发生 AT 响应超时，不是处理器异常。
- 已实现启动恢复：AT 同步超时时发送 `AT+WFQAP`，尝试取消 DA16200 使用已保存 AP 配置发起的自动连接，再重新同步；新增 `g_startup_cancel_attempts` 供 Watch 观察。
- 2026-09-26 Keil `Target_1` 构建成功，`0 Error(s), 0 Warning(s)`；Program Size 为 Code=6940、RO-data=760、RW-data=4、ZI-data=3500。最新版尚未烧录和实机验证。

最新已构建产物 SHA-256：

```text
7C6B4CC0FB3977CDCA5845D796DF66BF526430429D463DE7C756A300978F92F3  E:\RenesasProject\RA4M2_Blink\src\hal_entry.c
2CCE07919B5E4E062C9A1E88F504599E56BFCE6C08CE6E8A21B12C56DA40F627  E:\RenesasProject\RA4M2_Blink\configuration.xml
AD6179A426874D48895637C7E2B0ADCFC4F7E14E192F17018EAE3B4E7BCF471D  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
D05222D5C04067980A439892BE2E77C06D876014F31F01359DF175DA256AEDAF  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
7CB7C4973DA5EA24D79D11FAE2D77694E820739967D06E4824D60D4404A271A7  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 2026-09-27 DPM 自动硬件唤醒补录

- DA16200 继续使用其自身保存的 Station Profile、Country Code `CN`、DHCP、SNTP 和 DPM 配置；用户确认模块可以自动连接热点、获得 IP、同步时间并进入 `Start DPM Power-Down !!!`。当前 RA4M2 固件不再主动扫描、入网或改写 DA16200 的 Wi-Fi、Country Code、Profile、`AT+DPM` 或 NVRAM配置。
- 用户实测 DA16200 `J1-6` 为 `RTC_WAKE_UP`，DPM Power-Down 时约 3.3 V；短暂拉低后释放会输出 `Wakeup source is 0x81` 并唤醒，确认 HIGH 空闲、下降沿触发。
- RA4M2_SENSOR 原理图与 RASC 资源核对确认 `P102` 引出到 `CN8-1` 且修改前空闲，不与 SCI0（P101 TX/P100 RX）、SWD/JTAG、P103 LED或按键冲突。`configuration.xml` 已把 P102 配置为普通 GPIO Output、初始 HIGH，RASC 对应生成 `BSP_IO_PORT_01_PIN_02` 输出高电平。
- 用户在连接前测量确认 P102 HIGH，随后连接 `RA P102/CN8-1 → DA J1-6/RTC_WAKE_UP` 并保持公共 GND。源码产生 `HIGH → LOW 1 ms → HIGH` 脉冲，等待 `+INIT:WAKEUP,EXT` 后执行 `AT+MCUWUDONE → AT+CLRDPMSLPEXT → AT → AT+SETDPMSLPEXT`。
- 最终 Keil Watch 证据为 `g_wakeup_pulse_count=1`、`g_wakeup_stage=6`、`g_wakeup_gpio_result=0`、`g_dpm_handshake_stage=5`、`g_mcuwudone_result=1`、`g_clear_dpm_sleep_result=1`、`g_at_sync_result=1`、`g_set_dpm_sleep_result=1`、`g_at_attempt=4`、`g_uart_error=0`，最后响应行为为 `OK`；接收无非法字节、丢弃或行溢出。恢复 DPM 后的 `g_last_uart_event=0x50` 为 Framing Error + Break Detect 组合事件，不否定此前握手成功。
- 用户确认人工重复验证通过，并决定不执行 100 次自动循环。因此本阶段仅记录为“DPM自动硬件唤醒与最小UART会话实机通过”，不表述为压力测试、长期稳定性或完整功耗验收。
- 最新 `Target_1` 由 Arm Compiler 6.24 构建成功，`0 Error(s), 0 Warning(s)`，Program Size 为 Code=6404、RO-data=688、RW-data=4、ZI-data=2744。Codex未执行烧录，实机烧录与Watch验证由用户完成。

2026-09-27 当前产物 SHA-256：

```text
A8D2B12A3638F3956E567E837FD1C320DAE758F01571459E07F7D1858AFA5EC8  E:\RenesasProject\RA4M2_Blink\src\hal_entry.c
3049B133CE36FAF19CDED0D691191E59063A45BD6AB8EF8BF0E4C0C3D93568FA  E:\RenesasProject\RA4M2_Blink\configuration.xml
B0CA570188987DC54D8650359F151C32FC06715407969535E772D90F6BDA4431  E:\RenesasProject\RA4M2_Blink\ra_gen\pin_data.c
4DA7801AEF89E4FCE66AEB3AAD939124E4D73F3CD9587CEDD64C689443C7C62E  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
52304E1CAC7020FF0FE5276E529B5867D40C1E470A4AA6BCB2555D38396196ED  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
8C14EFE4E387AD59DF10E1CA97C3DFA0CA4767CA03FB84E5DBDF1E901445C21E  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 2026-09-28 ADC 与 SSD1306 OLED 实机验证补录

- RASC/FSP 6.6.0 已配置 `g_i2c_oled`，使用 SCI2 Channel 2 的 Simple I2C：P301/SCI2_SCL、P302/SCI2_SDA，7-bit地址 `0x3C`，Standard 100 kbit/s，回调优先级12，DTC关闭。
- 用户确认RA4M2-SENSOR板卡R28/R29选路焊接正确，并完成OLED接线：GND→CN4-3、VCC→CN4-2（3V3）、SCL→CN4-4、SDA→CN4-5。
- `src\OLED.c/.h/.Font.h`完成SSD1306驱动移植，采用FSP异步I2C回调、有限超时、Abort处理和批量字符发送。`hal_entry.c`将P013/ADC0 AN011电位器值显示到OLED，并在DA16200启动等待/重试期间每200 ms按变化刷新。
- 用户Keil Watch截图显示OLED状态正常：`g_oled_status=1`、`g_oled_last_fsp_error=0`、Abort和Timeout均为0；ADC截图值为2741、66.9%。用户随后确认转动电位器时OLED无需复位即可实时更新。
- 2026-09-28全量Rebuild：Arm Compiler 6.24，`0 Error(s), 0 Warning(s)`；Code=13040、RO-data=2948、RW-data=4、ZI-data=3352。用户完成烧录并实机确认，本次Codex未执行烧录。

本次补录产物哈希：

```text
1C3B428B30B023231A7057C8824C32A9FAE6BFAAE6DC360D1D7D0646D9819DF4  E:\RenesasProject\RA4M2_Blink\src\hal_entry.c
07784A80507B879BDE6F0F78F0FB7A29FC1837E56718EC241B40E92DAEEB9147  E:\RenesasProject\RA4M2_Blink\src\OLED.c
F5A35D3243486ACCB9815E5649AB7AC5EE782AED348EBD52E2370FF960040BF4  E:\RenesasProject\RA4M2_Blink\src\OLED.h
C3E56E043A461354E12E629070C4D10C9B1BF09645776B34B6C8321C7B790A2C  E:\RenesasProject\RA4M2_Blink\src\OLED_Font.h
91FADF435E38D3E098B64F2392D3ABC11362FA7DF4241518A799DC743E3D32E0  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
7DD75A5B9E65B14199412D46B8EEFA0FA625472AAA2BF19A2FF894D220D60222  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
3329C01B607C5FA417DDECA76F20A8E6DB0CE80331011CF8CB972400B275A84E  E:\RenesasProject\RA4M2_Blink\Objects\codex_oled_refresh_rebuild.log
```

本项状态为：已实现、已构建、已烧录（用户完成）、已实机验证；尚未进行长期稳定性、100次循环、功耗或整机验收。

## 2026-09-28 DA16200 自动联网状态识别修复补录

- 用户现场确认 DA16200 上电后始终保持与手机热点连接；旧逻辑未先读取模块实际连接状态，而是直接执行 `AT+WFSCAN`。当扫描结果未匹配目标 SSID 时，RA4M2 将已联网模块误判为失败，表现为 `g_join_result=4`、`g_wifi_ready=0`，并在重试循环中反复触发 `RTC_WAKE_UP`，DA16200 串口随之重复输出 `rtc1 wakeup interrupt ...`。该现象不是热点掉线或凭据错误。
- `src\modules\da16200\da16200.c` 已在扫描/入网前增加 `AT+WFSTA` 查询：`+WFSTA:1` 时直接置 `g_wifi_ready=1` 并跳过扫描和重复入网；`+WFSTA:0` 或查询未得到有效连接状态时才进入原扫描/连接路径。新增 Watch 变量 `g_wifi_status_result` 和 `g_wifi_status`，其中 `g_wifi_status=1` 表示已连接、`2` 表示未连接。
- 2026-09-28 Keil `Target_1` 使用 ArmClang 构建成功，目标器件 `R7FA4M2AD`，结果 `0 Error(s), 0 Warning(s)`；Program Size 为 Code=13780、RO-data=3012、RW-data=4、ZI-data=3548。构建前 RASC 自动执行；构建前后内容哈希对比显示 `ra_gen`、`ra_cfg` 未产生新的内容变化，`via\rasc_armclang.via` 仅调整两个宏定义的顺序。
- 用户使用包含新状态变量的固件进入 Debug 并 Run。最终 Watch 显示 `g_at_sync_result=1`、`g_wifi_status_result=1`、`g_wifi_status=1`、`g_wifi_ready=1`、`g_wakeup_stage=6`；同时 `g_scan_result=0`、`g_scan_ap_count=0`、`g_join_command_result=0`，证明模块已联网时正确跳过扫描和重复入网，而不是相关步骤失败。
- 同一轮运行中，Watch 最后完整响应为 `g_last_rx_line="+NWMQMSGSND:1"`；DA16200 串口连续显示 Msg_ID 20、21、22 的 `Mqtt Pub Enq : SUCCESS`。这证明本轮三条 MQTT 消息均被模块成功接受进入发布队列；未提供对应三条云端页面记录，因此本轮新增证据不单独扩展为三条消息均已在云端持久留存。

本项状态为：已实现、已构建、已由用户运行并通过 Keil Watch/DA16200 串口验证。Wi-Fi 自动连接状态识别和本轮 MQTT 连续入队通过；断网恢复、热点重启、弱信号、消息持久化、100次循环、长期稳定性和功耗仍未验收。

本次补录产物 SHA-256：

```text
6AEFB0E028D27C5104EE985E18F68951C2A5D8CBD6D97A120EE13283BCC2B621  E:\RenesasProject\RA4M2_Blink\src\modules\da16200\da16200.c
122DC6CD7A9140CD48773A98DD0FB58DC6E74A9B7488F1F77CFECCFE115C6078  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
812FB85CF83ADB613CF4C0954CF0B0A58D2966F02B7C5D73D44C60D5F4EA44E3  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
BD2677988E445362C71A61FED5F9F3ED8F3768A77DF2B12404788BCEAB61638D  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 2026-09-28 DA16200 上电启动、重连与代码精简补录

- 用户确认此前每次重新上电后 DA16200 会停在 `[BOOT]`，必须通过调试控制台输入 `boot` 才继续启动；同时存在 VOFA 串口侧影响和 RA4M2 首次唤醒/AT 发送早于 DA16200 完成启动的问题。当前固件在 SCI0 首次打开后保持 TX 空闲 5 s，再执行 `RTC_WAKE_UP` 和 AT 流程；该上电自动启动/联网问题已由用户确认解决。
- Wi-Fi 失败不再在 `DA16200_Connect()` 内无限重试。连接尝试会返回应用主循环：未连接时每 2 s 触发下一次尝试，已连接时每 30 s 复查。用户确认热点断开后能够重新连接；该结果是一次现场功能确认，不等于弱信号、反复掉线或长期稳定性验收。
- 用户明确不再使用巴法云。当前工程已删除 `cloud_config.h`、`cloud_local_config.h`、历史数据回放、MQTT 配置/发布逻辑及其 Keil 工程引用；`wifi_credentials.h` 继续保留并只承担热点凭据配置。
- `da16200.c` 删除未启用的版本、国家码、DPM只读查询分支及对应废弃状态变量，保留 UART 环形缓冲、AT超时、DPM握手、Wi-Fi状态、扫描/加入和必要 Watch 诊断。当前文件为 849 行。
- 精简后的 `Target_1` 使用 ArmClang 6.24 构建成功，目标器件 `R7FA4M2AD`，结果 `0 Error(s), 0 Warning(s)`；Program Size 为 Code=12200、RO-data=2624、RW-data=4、ZI-data=3124。生成 AXF 和 HEX。本次精简版状态为“已实现、已构建”，尚未由用户重新烧录和实机复验。

本次精简版 SHA-256：

```text
1F6A9C337222B769E17BA6BE1B1461794C8C5EA141AE9EB93988E68173436A07  E:\RenesasProject\RA4M2_Blink\src\modules\da16200\da16200.c
E9314B9AACCD8753EAACB5F53528D3404CCD209A6626C9ACB4C90910264062AA  E:\RenesasProject\RA4M2_Blink\src\config\app_config.h
C64C06A69DAE063B0799E0A750779191A2F5F4FE590639331545866DD0F7CB53  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
E9056F733F250D4CA94DBC308330F5A5B04F3E3AA8FD38BC1EF729E16166A8E8  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
50ACC3C660080456026493FB5B6AFB5237C14963C4288AD0E9FF960B6EB23FBD  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 2026-09-29 DHT11 实时采集、引脚迁移与工程精简补录

- 实物为带小板的三针 DHT11 模块，接口丝印从左到右为 GND/DATA/VCC；用户已确认供电和连线：GND→RA GND、VCC→RA 3.3 V、DATA→P015（CN8-5）。模块 DATA 与 VCC 之间实测约 9.x kΩ，可判定小板已带约 10 kΩ 上拉，因此未增加外部上拉，也未启用 RA 内部 Pull-up。
- 初始选用的 P104 受板载 USER LED D6 与 R4 4.7 kΩ 负载影响，不适合作为 DHT11 单总线数据脚。P104 的 DHT11 配置已解除，最终在 RASC 中将 P015 配置为符号名 `DHT11_DATA`、GPIO Input、Pull-up=None、IRQ=None、Drive Capacity=Low、CMOS；生成代码对应 `BSP_IO_PORT_00_PIN_15`。
- `src\modules\dht11\dht11.c/.h` 已实现单总线读取：主机起始低电平 20 ms、释放 13 µs、读取 40 bit、校验和验证、有限超时及 Watch 诊断。应用层采样周期为 2500 ms，并在 OLED 显示温湿度；未接入联网发送。
- 用户调试截图显示 `g_dht11_status=0x02`、`g_dht11_read_count=2`、超时和校验错误计数均为0，随后明确确认10分钟稳定性测试通过。该结果适用于当时已烧录固件；不能代替本轮代码精简后新产物的重新烧录回归。
- 工程精简删除了主循环中的重复电位器采样、无引用测试钩子及其Keil工程项，并简化OLED显示缓存状态；保留DA16200诊断、DHT11诊断、OLED通用显示函数和即将使用的MPU6050占位模块。共享I²C总线层延后到正式接入MPU6050时处理，DA16200阻塞式连接流程暂不调整。
- 仓库根目录 `.gitignore` 已明确忽略 `/RA4M2_Blink/src/config/wifi_credentials.h`，热点敏感信息仍仅保存在本地，不纳入版本控制或交接文档。
- 2026-09-29 `Target_1` 使用 ArmClang 6.24 构建成功，目标器件 `R7FA4M2AD`，结果 `0 Error(s), 0 Warning(s)`；Program Size为Code=13684、RO-data=2632、RW-data=4、ZI-data=3164，生成AXF和HEX。本轮状态为“已实现、已构建”，尚未重新烧录或完成精简后的实机回归。

本轮当前产物 SHA-256：

```text
9FD7DBDC151A617786D76513A9BCDF63BC0DB2B129D39EB605C556D165E87AD1  E:\RenesasProject\RA4M2_Blink\src\app\app_main.c
186E6E071E3BA730A9391AF04F67ED463FB06002F9F1E9758343F729E92F9B38  E:\RenesasProject\RA4M2_Blink\src\modules\dht11\dht11.c
6989141CC5CDE50850E81095957EA295423B9F7131D81D22AEE0ED255915DB32  E:\RenesasProject\RA4M2_Blink\src\modules\dht11\dht11.h
B976235BBD6A89306F6C41E424B7D0E1F8347436AE998923564892D94C00672D  E:\RenesasProject\RA4M2_Blink\configuration.xml
9B1A1E198642A89E14152288F2D2CB375BAD0CD61D523AAE84D53A9828029FA6  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
0C999B464B8EA47FDE609ED576A40770AF782955D241F1497FDE72942428BB1E  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
5F3091830B626E0230BB7BC58563451A5932B4CD5CC5FB75F58AB1B8D6338172  E:\RenesasProject\RA4M2_Blink\Target_1_build.log
```

## 2026-09-29 MPU六轴、姿态解算、OLED与倾斜告警补录

- 用户确认所用模块外部以5 V供电，I²C逻辑电平为3.3 V；SCL/SDA与OLED共用P301/P302，AD0默认低电平，对应7-bit从机地址`0x68`；模块板载SCL/SDA各4.7 kΩ上拉。以上属于用户提供的硬件事实，本轮没有新增RASC配置。
- 用户提供的STM32F103成功工程将从机地址定义为`0x68`，同时允许`WHO_AM_I`返回`0x68`或`0x70`。因此该工程文本中的“寄存器地址0x70”实际是身份寄存器返回值，不是I²C从机地址。
- 当前工程新增`src\modules\i2c_bus`共享访问层，由同一个SCI2 Simple I²C实例按事务切换OLED `0x3C`和IMU `0x68`；MPU路径实现身份读取、唤醒、采样率/滤波/量程配置回读、14字节六轴与温度连续读取。实机返回`WHO_AM_I=0x70`，固件按MPU6500兼容身份处理，不再把该值误判为总线故障。
- 第一轮Watch截图显示`g_mpu6050_status=0x02`、`g_mpu6050_pwr_mgmt_1=0x01`、`g_mpu6050_who_am_i=0x70`、`g_mpu6050_last_fsp_error=0`、`g_mpu6050_read_count=0x55`、`g_mpu6050_error_count=0`，证明当前接线下初始化和连续读取通过。后续截图显示校准完成、读取计数`0x16E`、错误计数0；换算后的加速度合量约0.96 g、IMU温度约32.5 ℃，静止陀螺仪各轴约在1 °/s以内。
- 驱动已完成±2 g、±2000 °/s物理量换算、200样本上电静止零偏校准，并以50 Hz执行Roll/Pitch互补滤波；没有磁力计，未实现可长期稳定的Yaw。用户明确确认姿态解算实机测试通过。
- OLED保留DHT11/ADC原页面，并每3 s切换到Roll、Pitch、IMU温度和倾斜状态页面；姿态计算保持50 Hz，OLED姿态页以2 Hz刷新。用户确认双页面、角度/温度刷新及共享I²C实机运行通过。
- 倾斜判定采用进入阈值30°并持续500 ms、恢复阈值20°以内并持续1 s；20°～30°为迟滞区，通信异常时状态为未知。用户确认正常、告警、迟滞保持和恢复流程通过。
- 最新`Target_1`由ArmClang 6.24构建成功，结果`0 Error(s), 0 Warning(s)`；Program Size为Code=18156、RO-data=2672、RW-data=8、ZI-data=3316，生成AXF和HEX。用户确认本轮功能实机通过；未执行长期、压力、弱信号、功耗或巡线机器人整机验收。

本轮当前产物 SHA-256：

```text
BB713039865A9AA6EB12A1278E35907C2240B5610461C68024278BD8A8437848  E:\RenesasProject\RA4M2_Blink\src\app\app_main.c
3E91942897E51180FE29B8370EB150783CE9ABF2AEDD215FD797B42C972CA465  E:\RenesasProject\RA4M2_Blink\src\config\app_config.h
27B24E21CF35774D3995FF2892E3CC3F29177C552822760C0C43DE0120C7A789  E:\RenesasProject\RA4M2_Blink\src\modules\mpu6050\mpu6050.c
61961E63D697BA6B2F79272EE0C6EECE2D680EB7FC5E9FF95C9B38DF8E5BE471  E:\RenesasProject\RA4M2_Blink\src\modules\mpu6050\mpu6050.h
6544D7EA4E54E4D65F833E63D17E47E128AD1F9D8F6F2C71916950B66FF6852E  E:\RenesasProject\RA4M2_Blink\src\modules\i2c_bus\i2c_bus.c
89B0349728F58D7DAE65B1E9ECEF279093FB605B66153CA01C2250D71D5E464C  E:\RenesasProject\RA4M2_Blink\src\modules\i2c_bus\i2c_bus.h
EB0FBFF333CDBCD3354365E5A31BCE7E0F365146667C503E5CA5C68C2E057D0D  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.axf
CB83DCED5AE4D8368EB3505CA33855833808F0D5384C78EFFF34B42F099F74F7  E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.hex
4AD8FAB02D30B6EE101658C250961DB99999505AE3D8EEDAE9A1A77D5D6C5310  E:\RenesasProject\RA4M2_Blink\mpu6050_build.log
```

## 下一步边界

RA4M2 ↔ DA16200基础连接、DHT11、ADC/OLED以及IMU共享I²C、六轴采集、Roll/Pitch、双页面显示和倾斜告警均已有用户实机证据。当前固件不包含巴法云、阿里云Alink、MQTT、历史数据重放或实时传感器消息发送。

后续路线已确定为：先建立与云平台无关的`app_telemetry_t`统一快照，集中保存DHT11温湿度、ADC原始值/百分比、IMU温度、Roll/Pitch、倾斜状态、序号/运行时间及各数据源有效性；温度、湿度、百分比和角度在本地使用扩大10倍的定点整数。先通过Keil Watch仅观察`g_app_telemetry`，核对其与当前OLED及各驱动结果一致。完成本地验证后，再依次定义阿里云物模型、实现“本地定点整数→阿里云直观数值”的Alink JSON映射，最后恢复DA16200 MQTT发送。`ProductKey`、`DeviceName`、Topic和认证信息不得进入统一快照，凭据不得写入交接报告或受版本控制的源码。

统一快照和字段映射属于用户代码修改，不涉及RASC；网络发送继续后置。弱信号、反复掉线、功耗和更长时间稳定性仍待单独验收；用户明确不执行100次循环测试。

## 证据文件

- 构建命令日志：`E:\RenesasProject\RA4M2_Blink\Target_1_build.log`
- 本轮MPU/姿态构建日志：`E:\RenesasProject\RA4M2_Blink\mpu6050_build.log`
- Keil 构建日志：`E:\RenesasProject\RA4M2_Blink\Objects\RA4M2_Blink.build_log.htm`
- 工程报告：`E:\RenesasProject\RA4M2_DA16200巡线机器人替代方案交接报告_2026-09-24.md`
