# BW20 纸钞盒移植说明

业务基准：ESP32C2_ZCJ_V1.0.4；对端协议依据：SG0B1_STD_V1.0.6。固件版本：BW20_ZCJ_V1.0.0。STM32 工程未修改。本分支用于配合 STM32 的纸钞盒。

## 硬件与启动

- 按 BW20-WIFI_Cash_Box_STHW V1.00 原理图，模块与 STM32 使用 UART0，TX=PA30、RX=PA31，固定 115200、8N1。机台波特率是另一个保存参数，不能改变此链路。
- 配网按键 PA27；Wi-Fi 配网模式信号 WIFI_AIRMOD=PA26。此信号不作为机台脉冲极性。LOG UART 用于工厂提示；不占用屏幕口。
- 无设备号时进入工厂模式，每 4 秒发送参考版 ASCII 提示，接收 `write device_id:`，设备号允许 4～32 字节及原有字符集。保存成功后回参考版成功字符串并复位。
- 保留 BW20 原设备号、Wi-Fi 和服务参数文件；兼容旧 32 字节设备号。输出方式/端口保存在 ZCJ_PULSE_INFO。
- 默认设备类型仍为 dev_type_default，与参考版一致；平台设置 dev_type_uart_note_lyy 后运行纸钞业务，脉冲/电平模式也经 STM32 执行。
- 纸钞机、机台输出、现金检测及屏幕物理接口由 STM32 执行；BW20 不再注册本地投币/退礼 GPIO 业务。

## 业务链路

- 支付：云服务→异步事件→D6/05 申请→D6/06 启动接单→云服务回复；D6/07 最终结果另发 start_result。
- 现金：STM32 D6/08→先回 unique/channel/01→上一流水号去重→参考格式 accounts 或 accounts_pulse_coin。开机异步读取 D6/0C 总账。
- D6/0B 回复设备类型、机台波特率、灯状态、脉宽/间隔及输出端口；配网模式下也保留应答。
- D6/02、03 参数读写；D6/10 MCU 版本；D6/01 屏幕透传。流式接收处理半包、粘包及校验错误。
- 修正参考源码中参数设置 ACK 未唤醒、启动应答 u32 结果布局、订单缓冲读取越界、首笔现金流水恰为 03 被误去重、LEVEL 缺启动分支等问题。
- 请求串行化，支付应答匹配订单及通道；不匹配当前请求的应答不唤醒等待任务。

## 升级

- module=default：BW20 自身 OTA，继续使用 Realtek OTA 镜像。
- module=board：保留参考版 CD 旧主板升级链路。
- module=zcjBoard：STM32 D6/0E 专用升级，文件缓存下载，检查实际大小，1024 字节分包并等待匹配名称/总长/偏移/长度/结果的 ACK；读失败或超时停止。不会误进入 BW20 自升级。
- 不覆盖 BW20 固定 Flash 地址；ESP 固定地址只作为缓存接口的虚拟地址。
- STM32 只接受严格递进偏移；ACK 丢失时停止，需要重新发起完整升级。最终块后对端会复位，必须实机确认新版本，不能把传输结束视为已完成升级。

## 编译与测试

在工程根目录进入 WSL：

```sh
source ~/venu/bin/activate
cd amebadplus_gcc_project
./build.py
```

默认输出 `amebadplus_gcc_project/km0_km4_app.bin` 和 `ota_all.bin`。版本归档在 `lyy_app/firmware/BW20_ZCJ_V1.0.0/`；调试旧包 `km0_km4_app_debug.bin` 不属于本次输出。

主机回归测试（输出目录请使用工程之外的临时目录）：

```sh
bash lyy_app/tests/run_host_tests.sh /tmp/bw20-zcj-tests
```

测试使用 GCC 的 -Werror、AddressSanitizer 和 UndefinedBehaviorSanitizer，覆盖帧切分/粘包/校验/扩展长度/计时回绕、工厂烧号边界/分包/写失败/旧号兼容、真实缓存读写越界、STM32 OTA 分包与错误 ACK、D6 参数/订单/启动/账目及配置字段。

本次验证结果：按上述 WSL 流程 `./build.py` 返回 0，日志结尾为 `Build done`；五组主机测试全部通过；`git diff --check` 通过。APP=922240 字节、OTA=922272 字节，版本副本与默认输出 SHA-256 一致，校验值见归档目录 SHA256SUMS.txt。未执行烧录或实机收钞、支付、联网及升级验证。

## 烧录后验收与现有限制

编译和主机测试不能代替实机验证。至少检查：工厂提示经 STM32 到烧号口、烧号双方复位；配网两种方式；D6/0B 连续运行超过 35 秒不复位；各纸钞通道及脉冲账目；线上申请/接单/最终事件；参数保存和断电恢复；串口屏；BW20 与 STM32 两类 OTA。

保留参考版的现金 ACK/去重与事件语义；没有新增跨重启订单去重或断网增量事件持久队列，不能保证断网现金增量逐笔补报。累计账目仍从 STM32 获取。

对端参考源码的 device_hwzcj_pay_start_parse 存在把未初始化 number 转为地址的 memcpy，且最终结果上报固定成功；遵照要求未改 STM32。联调若出现启动异常，应确认实际烧录 MCU 固件与参考源码是否一致。BW20 只能按收到的对端结果上报。
