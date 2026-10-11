# RA4M2 与 DA16200 联调测试

这是一个面向 Renesas RA4M2 开发板与 DA16200 Wi-Fi 模块的联调测试仓库，集中展示板端程序、模块通信、联网及 MQTT 数据发布的实现。项目定位为学习与开发测试，不作为产品级固件发布。

## 测试范围

- **主线：RA4M2 + DA16200。** 工程包含 DA16200 的连接、状态检查及 MQTT 发布相关代码，用于展示板端与 Wi-Fi 模块的联调过程。
- **辅助：其他模块。** DHT11、MPU6050、电位器和 OLED 提供测试数据、状态显示及基础外设接口的验证场景；这些模块不是项目的主要目标。

仓库展示的是当前源码和配置，不代表所有硬件组合均已通过实机或长期稳定性验证。接线、供电、电平及可用功能取决于实际板卡、模块资料和测试环境。

## 工程位置

- `RA4M2_Blink/RA4M2_Blink.uvprojx`：Keil 工程文件。
- `RA4M2_Blink/configuration.xml`：Renesas 工程配置。
- `RA4M2_Blink/src/app/`：应用流程与测试数据组织。
- `RA4M2_Blink/src/modules/da16200/`：DA16200 模块通信代码。
- `RA4M2_Blink/src/modules/`：其他辅助测试模块。

真实 Wi-Fi 凭据不随仓库发布；本地配置文件 `RA4M2_Blink/src/config/wifi_credentials.h` 已被 `.gitignore` 排除。

## 许可证与第三方内容

项目中由维护者持有版权且有权授权的原创内容采用 [MIT License](./LICENSE)。Renesas FSP、Arm CMSIS 和 OLED 字库等第三方内容不因此转为 MIT；各自的来源与适用声明见 [第三方声明](./THIRD_PARTY_NOTICES.md)。
