# OpenPay 纯支付协议学习笔记（结合 BW20 项目）

资料来源：

- `OPEN_PAY POS支付协议V1.0.7.docx`
- `纯支付文档协议参考.rtf`
- 项目代码：`lyy_app/hw/lyy_device/lyy_czf.c`、`lyy_app/hw/lyy_device/lyy_device.c`、`lyy_app/hw/lyy_thing/thing_service.c`

## 目录

- [1. 先抓住整体角色](#1-先抓住整体角色)
- [2. 纯支付 IOT 盒子和其他 IOT 盒子的区别](#2-纯支付-iot-盒子和其他-iot-盒子的区别)
- [3. 串口配置](#3-串口配置)
- [4. 数据帧格式](#4-数据帧格式)
  - [4.1 Length 计算](#41-length-计算)
  - [4.2 Check 计算](#42-check-计算)
- [5. 项目里的收包流程](#5-项目里的收包流程)
- [6. 子命令速查](#6-子命令速查)
- [7. 重点命令拆解](#7-重点命令拆解)
  - [7.1 `0x01` 带价格申请支付二维码](#71-0x01-带价格申请支付二维码)
  - [7.2 `0x02` 盒子下发支付二维码](#72-0x02-盒子下发支付二维码)
  - [7.3 `0x03` QR 支付结果通知](#73-0x03-qr-支付结果通知)
  - [7.4 `0x04` 机器启动状态](#74-0x04-机器启动状态)
  - [7.5 `0x05 / 0x06` 获取支付渠道](#75-0x05--0x06-获取支付渠道)
  - [7.6 `0x07` 取消 POS 支付](#76-0x07-取消-pos-支付)
  - [7.7 `0x08` 心跳/在线状态](#77-0x08-心跳在线状态)
  - [7.8 `0x09` 退款流程](#78-0x09-退款流程)
  - [7.9 `0x0A` WiFi 账号密码设置](#79-0x0a-wifi-账号密码设置)
- [8. 云端事件和服务对应关系](#8-云端事件和服务对应关系)
  - [8.1 主板请求 -> BW20 -> 云端事件](#81-主板请求---bw20---云端事件)
  - [8.2 纯支付主板消息对应的云端日志](#82-纯支付主板消息对应的云端日志)
  - [8.3 云端服务 -> BW20 -> 主板](#83-云端服务---bw20---主板)
  - [8.4 云端属性 -> 本地支付渠道](#84-云端属性---本地支付渠道)
- [9. 一次完整支付流程](#9-一次完整支付流程)
  - [9.1 申请二维码](#91-申请二维码)
  - [9.2 云端下发二维码](#92-云端下发二维码)
  - [9.3 支付成功](#93-支付成功)
  - [9.4 取消订单](#94-取消订单)
- [10. 当前实现和协议文档的差异/风险点](#10-当前实现和协议文档的差异风险点)
- [11. 串口调试时的检查清单](#11-串口调试时的检查清单)
- [12. 常用样例](#12-常用样例)
- [13. 建议学习顺序](#13-建议学习顺序)

## 1. 先抓住整体角色

这个协议在项目里对应“纯支付 / POS 支付”设备类型，命令字是 `0xD7`。

通信链路可以理解成三段：

1. 主板/POS 通过串口向 BW20 盒子发起支付相关请求。
2. BW20 盒子把请求转成 MQTT 事件上报到乐摇摇云端。
3. 云端通过服务下发二维码、支付结果、取消结果，BW20 再封装成 `0xD7` 串口帧发回主板/POS。

项目中的关键入口：

- 串口总解析：`lyy_app/hw/lyy_device/lyy_device.c`
- 纯支付命令处理：`lyy_app/hw/lyy_device/lyy_czf.c`
- 云端服务下发到串口：`lyy_app/hw/lyy_thing/thing_service.c`
- 支付渠道参数保存：`lyy_app/hw/lyy_thing/thing_property.c`

## 2. 纯支付 IOT 盒子和其他 IOT 盒子的区别

纯支付 IOT 盒子通常不带屏幕，这只是外观上的区别。从代码职责看，它更像一个“支付协议网关”：负责把主板/POS 的支付请求转成云端事件，再把云端二维码、支付结果、取消结果转成 `0xD7` 串口帧发回主板/POS。

其他 IOT 盒子通常更像“支付 + 设备控制器”，除了支付，还要处理具体设备业务，比如货道、套餐、库存、出币、启动机器、故障、屏幕页面等。

| 对比项 | 纯支付 IOT 盒子 | 其他 IOT 盒子 |
|---|---|---|
| 主要职责 | 连接主板/POS 和云端支付 | 控制具体设备业务，如售货、出币、扭蛋、启动机器 |
| 是否带屏幕 | 通常不带屏幕 | 很多带屏幕，或至少有本地 UI 交互 |
| 支付二维码显示 | 主板/POS 负责显示，盒子只转发二维码 URL | 盒子/屏幕模块可能自己显示二维码 |
| 和主板通信 | 主要跑 `0xD7` 纯支付协议 | 跑各类设备协议，如娃娃机、扭蛋机、售货机等 |
| 云端支付申请 | 主板发金额给盒子，盒子上报云端申请二维码 | 盒子根据套餐、货道、按键等主动发起支付申请 |
| 支付成功后 | 盒子通知主板支付成功，主板自己决定启动/出货 | 盒子可能直接控制脉冲、货道、电机、继电器等 |
| 设备状态 | 更关注网络、支付、订单状态 | 还关注货道、库存、出币、启动结果、故障等 |

如果只看“支付”这一部分，两类盒子的区别不在云端支付能力，而在谁发起支付、谁显示二维码、谁执行支付后的动作：

| 支付环节 | 纯支付盒子 | 其他盒子 |
|---|---|---|
| 支付发起方 | 主板/POS 发金额和渠道给盒子 | 盒子根据套餐、货道、按键、屏幕选择发起 |
| 金额来源 | 主板/POS 通过 `0xD7/0x01` 传入 | 本地套餐、货道价格、云端配置、屏幕选择 |
| 二维码显示 | 盒子转发云端二维码 URL，主板/POS 负责显示 | 盒子自己或屏幕模块显示二维码 |
| 支付结果处理 | 盒子把支付成功通知主板/POS | 盒子可能直接启动设备、出币、打开货道、控制脉冲 |
| 启动/出货责任 | 主板/POS 或设备主控负责 | 盒子通常参与控制，或直接下发启动指令 |
| 支付完成后的上报 | 主板再告诉盒子启动成功/失败，盒子上报云端 | 盒子自己知道启动/出货结果并上报云端 |

纯支付盒子的支付链路更像这样：

```text
主板/POS -> 盒子：我要收多少钱，使用哪个渠道
盒子 -> 云端：申请二维码
云端 -> 盒子：二维码 URL、订单号
盒子 -> 主板/POS：把二维码给你
主板/POS：自己显示二维码
云端 -> 盒子：支付成功
盒子 -> 主板/POS：这个订单支付成功了
主板/POS：自己启动机器/出货
主板/POS -> 盒子：启动成功或失败
盒子 -> 云端：上报启动状态
```

其他盒子的支付链路更像这样：

```text
用户在盒子/屏幕/设备上选择套餐或货道
盒子 -> 云端：申请支付
云端 -> 盒子：二维码 URL、订单号
盒子/屏幕：显示二维码
云端 -> 盒子：支付成功
盒子：直接控制设备启动/出货/出币
盒子 -> 云端：上报执行结果
```

可以简单记：纯支付盒子也做支付，但它主要做“支付桥接”，不主导业务动作；其他盒子把支付作为设备业务流程的一部分，通常还要参与价格选择、二维码显示、启动/出货控制。

在项目里，纯支付设备类型是：

```c
DEVICE_TYPE_UART_CZF
```

纯支付主流程可以概括为：

```text
主板发起支付申请
    ↓
BW20 盒子上报云端 amount_get_qrcode
    ↓
云端下发二维码 qrcode_post
    ↓
BW20 盒子转发二维码给主板
    ↓
云端下发支付结果 pay_notice
    ↓
BW20 盒子通知主板支付成功
    ↓
主板上报启动状态
    ↓
BW20 盒子上报云端 pay_start_status
```

所以纯支付盒子本身通常不关心机器怎么启动、怎么出货、怎么扣库存，它更关注这些数据：

- 金额
- 支付渠道
- 二维码
- 订单号
- 支付结果
- 取消支付
- 心跳状态

其他盒子相关代码里会看到更多设备控制类服务，例如：

- `pay_apply`
- `pay_start`
- `out_coins`
- `device_position_info`
- `update_position_info`
- `position_pay_package`

这些服务说明其他盒子还要处理货道、套餐、库存、出币数、启动结果、设备故障、屏幕页面等。

注意：虽然纯支付盒子通常不带屏幕，但项目中仍然有一些复用代码会调用 `lyy_tjc_*` 函数，比如 `lyy_tjc_pay_url_show()`、`lyy_tjc_page_pay_reuslt()`。学习时不要被这些屏幕函数带偏，判断是否是纯支付关键看 `get_dev_type_code() == DEVICE_TYPE_UART_CZF` 分支，以及是否最终调用 `device_packet_czf_data_length()` 封装 `0xD7` 串口帧。

## 3. 串口配置

协议文档定义：

- 接口：串口，支持 RS485 / RS232 / TTL
- 波特率：`38400`
- 数据位：`8`
- 停止位：`1`
- 校验位：无

项目现状：

- `APP_main.c` 初始化 `HAL_UART_MAIN_BOARD` 时，如果读参数失败默认用 `115200`，否则使用参数里的波特率。
- 如果对接严格按协议文档来的 POS 主板，要确认设备参数里主板串口波特率是否配置成 `38400`，否则会出现“帧格式都对但完全收不到/乱码”的问题。

## 4. 数据帧格式

`0xD7` 纯支付帧格式：

| 偏移 | 字段 | 长度 | 说明 |
|---|---:|---:|---|
| `0` | `Head` | 1 | 固定 `0xAA` |
| `1` | `Length` | 2 | 小端，长度字段本身不算入；包含 `CMD + 子命令 + 业务数据 + Check` |
| `3` | `CMD` | 1 | 固定 `0xD7` |
| `4` | `SubCmd` | 1 | 子命令 |
| `5` | `Payload` | N | 子命令业务数据，可为空 |
| `5+N` | `Check` | 1 | 异或校验 |
| `6+N` | `End` | 1 | 固定 `0xDD` |

### 4.1 Length 计算

代码里 `device_packet_czf_data_length()` 的逻辑是：

```c
packLen = dataLen + 3;
```

其中：

- `dataLen` 是业务数据长度，不含 `SubCmd`
- `+3` = `CMD(1) + SubCmd(1) + Check(1)`

所以：

```text
Length = 1(CMD) + 1(SubCmd) + Payload长度 + 1(Check)
```

例子：心跳请求没有业务数据，只有 `CMD + SubCmd + Check`，所以长度是 `0x0003`：

```text
AA 03 00 D7 08 DC DD
```

### 4.2 Check 计算

代码使用 `check_xor_memory()` 做逐字节异或。

校验范围：

```text
Length低字节 ^ Length高字节 ^ CMD ^ SubCmd ^ Payload...
```

不参与校验：

- `Head`
- `Check` 自己
- `End`

例子，获取支付渠道：

```text
AA 03 00 D7 06 D2 DD
```

校验：

```text
03 ^ 00 ^ D7 ^ 06 = D2
```

## 5. 项目里的收包流程

收包主流程在 `lyy_DevDataParse()`：

1. 判断帧头 `pdata[0] == 0xAA`。
2. 判断命令 `pdata[3] == 0xD7`。
3. 按小端解析长度：

   ```c
   pack_len = pdata[2];
   pack_len <<= 8;
   pack_len += pdata[1];
   ```

4. 校验总长度：实际长度至少为 `pack_len + 4`。
5. 校验帧尾：`pdata[pack_len + 3] == 0xDD`。
6. 校验异或：`check_xor_memory(&pdata[1], pack_len + 1)`。
7. 进入 `std_packetParse()`，再按 `CMD = 0xD7` 分发到 `device_czf_packet_parse()`。

注意：`std_packetParse()` 里把 `package[4]` 当作 `sub_cmd`，也就是 `0xD7` 的子命令。

## 6. 子命令速查

| 子命令 | 方向 | 协议含义 | 当前代码状态 |
|---:|---|---|---|
| `0x01` | 主板 -> 盒子 | 带金额申请支付二维码 | 已实现 |
| `0x01` | 盒子 -> 主板 | 二维码申请受理结果 | 已实现 |
| `0x02` | 盒子 -> 主板 | 下发支付二维码 | 已实现 |
| `0x02` | 主板 -> 盒子 | 主板接收二维码应答 | 收到后忽略 |
| `0x03` | 盒子 -> 主板 | QR 支付结果通知 | 已实现成功通知 |
| `0x03` | 主板 -> 盒子 | 支付结果接收应答 | 收到后忽略 |
| `0x04` | 主板 -> 盒子 | 返回机器启动状态 | 已实现上报云端 |
| `0x04` | 盒子 -> 主板 | 启动状态接收应答 | 已实现 |
| `0x05` | 主板 -> 盒子 | 获取支付渠道 V1 JSON | 代码收到后仍走 HEX 返回 |
| `0x06` | 主板 -> 盒子 | 获取支付渠道 V2 HEX | 已实现 |
| `0x07` | 双向 | 取消 POS 支付/取消订单 | 已实现部分流程 |
| `0x08` | 主板 -> 盒子 | 心跳/在线状态 | 已实现 |
| `0x09` | 双向 | 退款流程 | 协议有，当前代码未实现 |
| `0x0A` | 双向 | WiFi 账号密码设置 | 协议有，当前代码未实现 |

## 7. 重点命令拆解

### 7.1 `0x01` 带价格申请支付二维码

方向：主板 -> 盒子

业务数据结构在 `DEVICE_GET_QRCODE_CZF_DEF`：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x01` |
| `amount` | 4 | 金额，单位为分，DWORD 小端 |
| `channelNumber` | 64 | 商户渠道编号，不足补 `0x00` |
| `wallet` | 32 | 钱包/支付渠道能力编码，不足补 `0x00` |
| `unique` | 6 | 6 位唯一标识 |

代码处理：

- `device_czf_qrcode_apply()` 解析金额：

  ```c
  amount[0] | amount[1] << 8 | amount[2] << 16 | amount[3] << 24
  ```

- 金额上报云端时除以 `100.0`，字段名是 `commodity_amount`。
- 同时上报：
  - `pay_channel_code` = `wallet`
  - `pay_channel_number` = `channelNumber`
  - `unique_code` = `unique`
- MQTT 事件名：`amount_get_qrcode`
- 随后立即回复主板：
  - `0x01`：MQTT 在线，受理成功
  - `0x00`：无网络/未连接

参考样例：

```text
AA 6D 00 D7 01
01 00 00 00
50 4F 53 5F 77 69 7A 61 ... 00
50 4F 53 ... 00
42 42 42 42 42 42
92 DD
```

其中：

- `01 00 00 00` = 1 分
- `50 4F 53 5F 77 69 7A 61` = `POS_wiza...`
- `50 4F 53` = `POS`
- `42 42 42 42 42 42` = `BBBBBB`

### 7.2 `0x02` 盒子下发支付二维码

方向：盒子 -> 主板

触发位置：云端调用服务 `qrcode_post`，进入 `thing_service_qrcode_dynamic_std()`。

业务数据：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x02` |
| `Len1` | 2 | 二维码 URL 长度，小端 |
| `url` | N1 | 二维码字符串 |
| `Len2` | 1 | 支付订单号长度 |
| `order` | N2 | 支付订单号 |
| `unique` | 6 | 唯一标识 |

代码来源字段：

- 云端服务参数 `url`
- 云端服务参数 `order`
- 云端服务参数 `unique_code`

当前代码中主板返回的 `0x02` 应答没有进一步处理。

### 7.3 `0x03` QR 支付结果通知

方向：盒子 -> 主板

触发位置：云端调用服务 `pay_notice`，进入 `thing_service_pay_result()`。

业务数据：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x03` |
| `result` | 1 | `0x01` 成功，协议里也定义了 `0x00` 失败 |
| `Len2` | 1 | 支付订单号长度 |
| `pay_order` | N2 | 支付订单号 |

项目现状：

- 代码读取云端字段 `status`，但当前下发给主板时固定写 `pay_result[0] = 0x01`。
- 也就是说当前实现主要覆盖“支付成功通知”，没有根据 `status` 映射失败。
- 主板返回 `0x03` 应答时，当前代码忽略。

### 7.4 `0x04` 机器启动状态

方向：主板 -> 盒子

业务数据：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x04` |
| `status` | 1 | `0x01` 成功，`0x00` 失败 |
| `Len2` | 1 | 订单号长度 |
| `pay_order` | N2 | 启动订单号 |

代码处理：

- `device_czf_start_status()` 把状态转成：
  - `start_status = 1`：启动成功
  - `start_status = 2`：启动失败
- 上报字段：
  - `pay_order`
  - `start_status`
- MQTT 事件名：`pay_start_status`
- 盒子回复主板 `0x04`，数据固定 `0x01`。

### 7.5 `0x05 / 0x06` 获取支付渠道

协议定义：

- `0x05`：JSON 方式返回
- `0x06`：HEX 结构方式返回

项目现状：

- `device_czf_packet_parse()` 收到 `0x05` 和 `0x06` 都调用 `device_czf_get_pay_channel()`。
- `device_czf_get_pay_channel()` 始终用 `0x06` 返回 HEX 数据，不返回 JSON。
- 必须满足 `tjc_infor_t.get_pay_channel_flag == 0x01` 才会返回；这个标志在云端属性 `pay_channel` 写入成功后由 `tjc_get_pay_channel_enable()` 设置。

HEX 返回业务数据：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x06` |
| `channel_count` | 1 | 渠道总数 |
| `number` | 1 | 渠道序号，从 1 开始 |
| `channel_number` | 64 | 商户渠道编号 |
| `code` | 32 | 钱包编码 |
| `name` | 32 | 钱包名称 |
| `pay_type` | 32 | 钱包类型 |

代码里 `DEVICE_PAY_CHANNEL_CZF_DEF` 总长度是 `161` 字节：

```text
1 + 64 + 32 + 32 + 32 = 161
```

代码注意点：

- `pay_channel_update()` 保存云端字段：
  - `code`
  - `channelNumber`
  - `channelCode`
  - `locale`
  - `paymentMethodTypeEnum`
- `device_czf_get_pay_channel()` 返回时复制：
  - `channel_number`
  - `code`
  - `pay_type`
- 当前没有把 `name` 复制到返回结构里，所以 `name[32]` 基本是空。
- `PAY_CHANNEL_MAX` 是 `4`，但 `chnl_sum` 来自云端数组长度；如果云端给超过 4 个，保存时会截断，后续返回时要注意不要越界。

参考请求样例：

```text
AA 03 00 D7 06 D2 DD
```

### 7.6 `0x07` 取消 POS 支付

协议里是双向都会用：

- 主板 -> 盒子：机器取消支付
- 盒子 -> 主板：POS 取消订单/自身取消订单通知

主板 -> 盒子业务数据：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x07` |
| `status` | 1 | 协议示例使用 `0x02` |
| `Len2` | 1 | 订单号长度 |
| `order` | N2 | 支付订单号 |

代码处理：

- `device_czf_pos_clear()` 解析订单号，上报 MQTT 事件 `pos_order_create_result_event`。
- 字段包括：
  - `order_no`
  - `transaction_number` 空字符串
  - `create_failure_reason` 空字符串
  - `create_result` 或 `create_resul`

注意：代码中 `data[1] == 0x02` 分支写的是 `create_resul`，少了末尾 `t`，这可能是云端字段兼容风险。

盒子 -> 主板方向：

- 云端调用 `pos_order_clear_func`。
- `thing_service_pos_clear()` 下发 `0x07`：
  - `clear_result[0] = 0x02`
  - `clear_result[1] = order_len`
  - 后面跟 `order_no`

### 7.7 `0x08` 心跳/在线状态

方向：主板 -> 盒子

请求没有业务数据：

```text
AA 03 00 D7 08 DC DD
```

盒子回复业务数据：

| 值 | 协议含义 |
|---:|---|
| `0x00` | 盒子不在线 |
| `0x01` | 盒子在线，POS 不在线 |
| `0x02` | 备用 |
| `0x03` | 盒子和 POS 都在线 |

项目现状：

- `device_czf_net_state()` 只判断 `lyy_mqtt_get_status()`。
- MQTT 在线就回 `0x03`，否则回 `0x00`。
- 当前没有单独检测 POS 是否在线，所以 `0x01` 暂未体现。

### 7.8 `0x09` 退款流程

协议定义了退款：

- 主板 -> 盒子：`0x09`，带订单号
- 盒子 -> 主板：`0x09`，返回 `0x02` 成功或 `0x03` 失败

项目现状：

- `device_czf_packet_parse()` 没有 `case 0x09`。
- `thing_service.c` 也没有对应退款服务下发到 `0x09`。

### 7.9 `0x0A` WiFi 账号密码设置

协议定义：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `SubCmd` | 1 | `0x0A` |
| `reserve` | 1 | 示例为 `0x02` |
| `ssid_len` | 1 | WiFi 账号长度，小于 64 |
| `ssid` | N1 | WiFi 账号字符串 |
| `pwd_len` | 1 | WiFi 密码长度，小于 64 |
| `pwd` | N2 | WiFi 密码字符串 |

参考样例：

```text
AA 14 00 D7 0A 02 05 41 41 41 41 41 09 41 41 41 41 41 41 41 41 41 C7 DD
```

表示：

- SSID = `AAAAA`
- 密码 = `AAAAAAAAA`

项目现状：

- `lyy_czf.c` 没有处理 `0x0A`。

## 8. 云端事件和服务对应关系

### 8.1 主板请求 -> BW20 -> 云端事件

| 子命令 | 代码函数 | MQTT 事件名 | 关键字段 |
|---:|---|---|---|
| `0x01` | `device_czf_qrcode_apply()` | `amount_get_qrcode` | `commodity_amount`、`pay_channel_code`、`pay_channel_number`、`unique_code` |
| `0x04` | `device_czf_start_status()` | `pay_start_status` | `start_status`、`pay_order` |
| `0x07` | `device_czf_pos_clear()` | `pos_order_create_result_event` | `order_no`、`create_result/create_resul`、`transaction_number`、`create_failure_reason` |

### 8.2 纯支付主板消息对应的云端日志

这部分是排查问题时最常用的对照表。主板发 `0xD7` 纯支付命令后，不是每个子命令都会产生云端日志：

| 子命令 | 代码函数 | 是否上报云端 | 云端事件名 | 后台日志重点看什么 |
|---:|---|---|---|---|
| `0x01` | `device_czf_qrcode_apply()` | 是 | `amount_get_qrcode` | 金额、渠道、钱包编码、`unique_code` |
| `0x04` | `device_czf_start_status()` | 是 | `pay_start_status` | 订单号、启动成功/失败 |
| `0x05` | `device_czf_get_pay_channel()` | 否 | 无 | 后台看不到事件，只会给主板回支付渠道 |
| `0x06` | `device_czf_get_pay_channel()` | 否 | 无 | 后台看不到事件，只会给主板回支付渠道 |
| `0x07` | `device_czf_pos_clear()` | 是 | `pos_order_create_result_event` | 订单号、取消/创建结果字段 |
| `0x08` | `device_czf_net_state()` | 否 | 无 | 后台看不到事件，只会给主板回在线状态 |

#### `0x01` 申请支付二维码：`amount_get_qrcode`

主板发起带金额的支付二维码申请时，进入：

```c
device_czf_qrcode_apply(data, len);
```

代码会上报云端事件：

```c
thing_event_post(value, "amount_get_qrcode");
```

上报字段：

| 云端字段 | 来源 | 说明 |
|---|---|---|
| `commodity_amount` | `amount[4]` | 商品金额，代码把“分”转换成“元”：`czf_amount / 100.0` |
| `pay_channel_code` | `wallet[32]` | 支付渠道/钱包能力编码，例如 `POS`、`HKWXZF` |
| `pay_channel_number` | `channelNumber[64]` | 商户渠道编号 |
| `unique_code` | `unique[6]` | 6 位请求唯一标识，用来串联申请和二维码下发 |

后台排查方法：

- 如果主板发了 `0x01`，后台没有 `amount_get_qrcode`，先看串口帧有没有进 `device_czf_qrcode_apply()`，以及 `cJSON_CreateObject()` 是否成功。
- 如果后台有事件但金额不对，重点看 `amount[4]` 是否按小端传入，单位是否是“分”。
- 如果后台有事件但云端没有下发二维码，重点看 `pay_channel_code`、`pay_channel_number`、`unique_code` 是否和云端渠道配置匹配。
- 盒子给主板的 `0x01` 回复只表示盒子当前 MQTT 是否在线：在线回 `0x01`，不在线回 `0x00`。

#### `0x04` 机器启动状态：`pay_start_status`

主板在收到支付成功后，自己启动机器/出货，然后把启动结果告诉盒子，进入：

```c
device_czf_start_status(data, len);
```

代码会上报云端事件：

```c
thing_event_post(value, "pay_start_status");
```

上报字段：

| 云端字段 | 来源 | 说明 |
|---|---|---|
| `start_status` | `data[1]` | `data[1] != 0` 上报 `1`，表示启动成功；否则上报 `2`，表示启动失败 |
| `pay_order` | `data[3]` 开始的订单号 | 支付订单号，长度来自 `data[2]` |

后台排查方法：

- 如果支付成功后后台没有 `pay_start_status`，说明主板可能没有发 `0x04`，或订单号长度 `data[2] > 63` 被代码直接丢弃。
- 如果后台显示启动失败，看主板发来的 `data[1]` 是否为 `0x00`。
- 如果订单号为空或不对，看 `data[2]` 长度和 `data[3...]` 内容是否匹配。
- 盒子收到后会给主板回 `0x04`，数据固定 `0x01`，表示盒子已接收。

#### `0x05 / 0x06` 获取支付渠道：不上报云端

主板获取支付渠道时，进入：

```c
device_czf_get_pay_channel(data, len);
```

这个函数不会调用 `thing_event_post()`，所以后台不会出现新的事件日志。它只是读取本地已缓存的支付渠道参数，然后通过串口回给主板：

```c
device_packet_czf_data_length(0x06, channel_data, ...);
```

数据来源是云端属性 `pay_channel` 之前下发后保存到本地的 `PAY_PARAM_DEF`：

```c
PAY_PARAM_DEF *pay_channel_t = get_pay_param();
```

返回给主板的字段：

| 串口字段 | 来源 | 说明 |
|---|---|---|
| 渠道总数 | `pay_channel_t->chnl_sum` | 可用支付渠道数量 |
| `channel_number` | `pay_channel_t->channel[i].channel_number` | 商户渠道编号 |
| `code` | `pay_channel_t->channel[i].code` | 钱包/支付渠道编码 |
| `pay_type` | `pay_channel_t->channel[i].pay_type` | 支付类型 |

排查方法：

- 如果主板发 `0x05/0x06` 后后台没日志，这是正常的。
- 如果主板收不到渠道，先看 `tjc_infor_t.get_pay_channel_flag` 是否为 `0x01`；不是 `0x01` 时函数直接 `return`。
- 再看云端属性 `pay_channel` 是否已经成功下发并被 `property_write_pay_channel()` 保存。
- 注意：代码里 `0x05` 和 `0x06` 都走这个函数，当前实际都按 `0x06` HEX 结构返回。

#### `0x07` 取消 POS 支付/取消订单：`pos_order_create_result_event`

主板取消支付或上报取消结果时，进入：

```c
device_czf_pos_clear(data, len);
```

代码会上报云端事件：

```c
thing_event_post(value, "pos_order_create_result_event");
```

上报字段：

| 云端字段 | 来源 | 说明 |
|---|---|---|
| `order_no` | `data[3]` 开始的订单号 | 订单号，长度来自 `data[2]` |
| `create_resul` | `data[1] == 0x02` 时写入 | 当前代码字段名少了末尾 `t`，值为 `2` |
| `create_result` | `data[1] != 0x02` 时写入 | 当前代码值为 `0` |
| `transaction_number` | 固定空字符串 | 代码传 `""` |
| `create_failure_reason` | 固定空字符串 | 代码传 `""` |

后台排查方法：

- 如果主板发 `0x07` 后后台没有 `pos_order_create_result_event`，先看订单号长度 `data[2]` 是否大于 `63`，大于时函数直接丢弃。
- 如果后台查字段时查不到 `create_result`，注意 `data[1] == 0x02` 分支代码写的是 `create_resul`，少了一个 `t`。
- 如果订单号不对，看 `data[2]` 和 `data[3...]` 是否匹配。

#### `0x08` 心跳/在线状态：不上报云端

主板查询盒子/POS 在线状态时，进入：

```c
device_czf_net_state(data, len);
```

这个函数不会上报云端事件，只会根据 MQTT 在线状态回复主板：

```c
if (lyy_mqtt_get_status())
{
    reply[0] = 0x03;
}
else
{
    reply[0] = 0x00;
}
device_packet_czf_data_length(0x08, reply, sizeof(reply));
```

排查方法：

- 如果主板发 `0x08` 后后台没日志，这是正常的。
- 如果主板收到 `0x00`，说明盒子判断 MQTT 不在线。
- 如果主板收到 `0x03`，说明盒子判断 MQTT 在线；当前代码没有单独检测 POS 是否在线。

### 8.3 云端服务 -> BW20 -> 主板

| 服务名 | 代码函数 | 下发子命令 | 关键字段 |
|---|---|---:|---|
| `qrcode_post` | `thing_service_qrcode_dynamic_std()` | `0x02` | `url`、`order`、`unique_code` |
| `pay_notice` | `thing_service_pay_result()` | `0x03` | `status`、`pay_order` |
| `pos_order_clear_func` | `thing_service_pos_clear()` | `0x07` | `order_no` |

### 8.4 云端属性 -> 本地支付渠道

云端属性 `pay_channel` 写入后，`property_write_pay_channel()` 解析 `pay_channel_list` 字符串，并保存到 `PAY_PARAM_DEF`。

常见 JSON 结构示例：

```json
{
  "pay_channel_list": "[{\"channelCode\":\"default\",\"channelNumber\":\"POS_wizar\",\"code\":\"POS\",\"name\":\"POS\",\"type\":\"POS\"}]"
}
```

代码实际读取的字段名包括：

- `code`
- `channelNumber`
- `channelCode`
- `locale`
- `paymentMethodTypeEnum`

这里要注意协议示例里的 `type` 和代码里的 `paymentMethodTypeEnum` 不完全一致；如果云端只下发 `type`，当前 `pay_type` 可能为空。

## 9. 一次完整支付流程

### 9.1 申请二维码

```text
主板/POS -> BW20: 0xD7 / 0x01，带金额、渠道、钱包、unique
BW20 -> 云端: amount_get_qrcode
BW20 -> 主板/POS: 0xD7 / 0x01，返回 0x01 或 0x00
```

学习重点：

- 金额单位是“分”，代码上报云端时转成“元”。
- `unique_code` 是串联请求和二维码下发的重要标识，代码固定取 6 字节。

### 9.2 云端下发二维码

```text
云端 -> BW20: qrcode_post(url, order, unique_code)
BW20 -> 主板/POS: 0xD7 / 0x02，下发二维码 URL、订单号、unique
主板/POS -> BW20: 0xD7 / 0x02，应答接收成功
```

当前代码没有处理主板的 `0x02` 应答，所以主板是否收到二维码主要靠串口层日志和设备行为确认。

### 9.3 支付成功

```text
云端 -> BW20: pay_notice(status, pay_order)
BW20 -> 主板/POS: 0xD7 / 0x03，通知支付成功和订单号
主板/POS -> BW20: 0xD7 / 0x03，应答接收成功
主板/POS -> BW20: 0xD7 / 0x04，机器启动结果
BW20 -> 云端: pay_start_status
BW20 -> 主板/POS: 0xD7 / 0x04，应答
```

学习重点：

- 支付成功不代表机器启动成功，`0x04` 才是机器执行结果。
- 当前 `pay_notice` 下发时没有根据 `status` 处理失败分支。

### 9.4 取消订单

主板主动取消：

```text
主板/POS -> BW20: 0xD7 / 0x07，带订单号
BW20 -> 云端: pos_order_create_result_event
```

云端/POS 取消通知主板：

```text
云端 -> BW20: pos_order_clear_func(order_no)
BW20 -> 主板/POS: 0xD7 / 0x07，带订单号
```

## 10. 当前实现和协议文档的差异/风险点

1. `0x05` JSON 渠道方式未真正实现  
   代码收到 `0x05` 后仍调用 HEX 渠道返回函数，并且返回子命令是 `0x06`。

2. `0x09` 退款未实现  
   协议文档有退款流程，但代码没有 `case 0x09`。

3. `0x0A` WiFi 配网未实现  
   协议文档和 RTF 参考都有 WiFi 设置样例，但代码没有处理。

4. `pay_notice` 失败状态未映射  
   代码读取了 `status`，但下发主板时固定 `0x01` 成功。

5. `0x07` 上报字段存在拼写风险  
   `data[1] == 0x02` 时写的是 `create_resul`，其他分支写 `create_result`。

6. 心跳状态只看 MQTT  
   协议有“盒子在线但 POS 不在线”的 `0x01`，当前代码只根据 MQTT 返回 `0x03` 或 `0x00`。

7. 支付渠道名称未返回  
   `DEVICE_PAY_CHANNEL_CZF_DEF` 里有 `name[32]`，但当前 `device_czf_get_pay_channel()` 没有复制名称。

8. 支付渠道字段名要和云端确认  
   协议例子有 `type`，代码读取的是 `paymentMethodTypeEnum`。如果字段不一致，`pay_type` 会为空。

9. 渠道数量要注意上限  
   `PAY_CHANNEL_MAX = 4`。云端给超过 4 个时，保存数据会截断，但 `chnl_sum` 可能仍是原数组长度，后续遍历有越界风险。

10. 波特率要对齐  
    协议是 `38400`，项目默认读参数失败是 `115200`。

## 11. 串口调试时的检查清单

调 `0xD7` 时优先按这个顺序看：

1. 帧头是否是 `AA`，帧尾是否是 `DD`。
2. `Length` 是否小端，是否等于 `CMD + SubCmd + Payload + Check`。
3. `CMD` 是否是 `D7`。
4. `Check` 是否从 `Length低字节` 开始异或到 payload 结束。
5. 子命令方向是否正确，比如 `0x01` 申请是主板发起，`0x02` 二维码是盒子下发。
6. 字符串字段是否按固定长度补 `0x00`，尤其是 `channelNumber[64]`、`wallet[32]`。
7. 订单号长度字段是否和实际字节数一致。
8. `unique_code` 是否固定 6 字节，并在申请和二维码下发中保持一致。
9. BW20 是否 MQTT 在线，`0x01` 和 `0x08` 回复都会受在线状态影响。
10. 支付渠道是否已经通过云端属性下发，否则 `0x05/0x06` 可能不返回。

## 12. 常用样例

获取支付渠道 HEX：

```text
AA 03 00 D7 06 D2 DD
```

心跳：

```text
AA 03 00 D7 08 DC DD
```

WiFi 配网协议样例（当前代码未实现）：

```text
AA 14 00 D7 0A 02 05 41 41 41 41 41 09 41 41 41 41 41 41 41 41 41 C7 DD
```

## 13. 建议学习顺序

1. 先手算 `AA 03 00 D7 06 D2 DD` 的长度和校验，确认帧格式理解正确。
2. 再看 `lyy_DevDataParse()`，理解串口包如何进入 `device_czf_packet_parse()`。
3. 看 `device_czf_qrcode_apply()`，理解主板支付申请如何变成云端 `amount_get_qrcode`。
4. 看 `thing_service_qrcode_dynamic_std()`，理解云端二维码如何回到主板。
5. 看 `thing_service_pay_result()` 和 `device_czf_start_status()`，把“支付成功”和“机器启动成功”区分开。
6. 最后对照第 9 节差异点，判断后续是否需要补实现退款、WiFi、失败通知、JSON 渠道返回。
