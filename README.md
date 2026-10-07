# RA4M2 与 DA16200 联调测试

本项目主要用于测试 Renesas RA4M2 开发板与 DA16200 Wi-Fi 模块，重点是板端程序运行、模块通信、联网及 MQTT 数据发布链路。它是学习和开发阶段的测试工程。

## 测试范围

- **主线：RA4M2 + DA16200。** 工程包含 DA16200 的连接、状态检查及 MQTT 发布相关代码，用于观察板端与 Wi-Fi 模块的联调过程。
- **辅助：其他模块。** DHT11、MPU6050、电位器和 OLED 用于提供测试数据、观察运行状态及验证基础外设接口；它们不是本项目的主要目标，也不代表独立产品功能已经完成验收。

源码和配置中存在相关实现，不等于所有硬件组合均已完成实机或长期稳定性验证。实际接线、供电、电平及可用功能应以所用板卡、模块资料和现场测试为准。

## 工程位置

- `RA4M2_Blink/RA4M2_Blink.uvprojx`：Keil 工程文件。
- `RA4M2_Blink/configuration.xml`：Renesas 工程配置。
- `RA4M2_Blink/src/app/`：应用流程与测试数据组织。
- `RA4M2_Blink/src/modules/da16200/`：DA16200 模块通信代码。
- `RA4M2_Blink/src/modules/`：其他辅助测试模块。

本地 Wi-Fi 凭据文件 `RA4M2_Blink/src/config/wifi_credentials.h` 已被 `.gitignore` 排除；请勿将真实凭据提交到仓库。

## 许可证与第三方内容

有权授权的自有内容采用 [MIT License](./LICENSE)。Renesas FSP、Arm CMSIS 和 OLED 字库等第三方内容不因此转为 MIT；各自的来源与适用声明见 [第三方声明](./THIRD_PARTY_NOTICES.md)。
