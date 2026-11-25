## 目标和范围
新增 `dev_type_pulses`：货道 1 → `COIN_OUT`（`PA_8`）、货道 2 → `GIFT_OUT`（`PB_21`）。沿用已经修改的 GPIO 基础，不回退工作区现有改动，也不改变原 `dev_type_pulse`、串口扭蛋机或 `COIN_IN` 的既有流程。不修改云端或屏幕固件。

## 1. 注册设备类型并同步云端数据
- 在 [设备类型定义](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_user/lyy_para.h)、[类型映射](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_user/lyy_para.c) 和 [物模型映射](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_thing/thing_iot.c) 注册 `dev_type_pulses`，保持旧类型的编号和值不变。
- [联网初始化](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_thing/thing_init.c) 为新类型发 `query_position_info`。由 [属性处理](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_thing/thing_property.c) 接收 `property.set.params.position_info`：外层为数组，内层为 JSON 字符串；兼容数字和字符串形式的 `positionX`，只收货道 1、2（`positionY=0`），分别保存 `positionName`。
- 成功解析货道信息后再发 `query_position_pay_package` 请求全部消费套餐，不用固定延时冒充同步完成。未收到有效货道信息时不开放购买，并按连接流程重试货道查询；重复下发不反复触发首轮套餐查询。
- 接收 `position_pay_package`，按 `positionList[].positionX` 分组、读取 `consumptionPackage`，每路最多 6 个。更新后空套餐清掉旧数据，不让一个货道覆盖另一个货道。保留币种和现有套餐显示格式；套餐 ID 按字符串保存，不丢精度。

## 2. 屏幕套餐页和取码
- 在 [屏幕流程](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_screen/lyy_tjc.c) 为新类型添加双货道显示。每次进入套餐页默认货道 1；`t1` 显示货道 1 名称，`t3` 显示货道 2 名称。`position1` 显示 `t0`、隐藏 `t2`；`position2` 隐藏 `t0`、显示 `t2`。按现有串口帧格式接收两条切换命令，在同一页面刷新，隐藏空余套餐位。
- 复用 `package1`～`package6`。按“当前货道 + 套餐槽位”选套餐，取码时保存该次选择；后续切换标签不会改变已经发出的取码请求。空槽位不发起取码。
- 在 [事件上报](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_thing/thing_event.c) 沿用 `get_qrcode`，带上数值 `position_x`、`position_y: 0` 和现有支付渠道字段。将通过校验的套餐 ID 原样序列化为 **不带引号的 JSON 数字**（SDK 支持 `cJSON_CreateRaw`），不经过 `double`，例如 `1574796066259251200`。不改旧设备的 ID 序列化方式。

## 3. 云端启动和两路脉冲
- 在 [启动服务](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_thing/thing_service.c) 和 [启动任务](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/hw/lyy_user/lyy_start.c) 增加新类型分支。校验 `pay_start` 的订单、货道和正整数 `number`，把货道和次数一并交给启动任务；无效货道或非法次数不输出，不默认启动货道 1。启动只依据云端 `position_x`，与屏幕选择、套餐 ID 无关；只在请求被输出逻辑接受后回报成功，不把收到请求等同于已出货。
- 在 [脉冲实现](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/app/APP_Pluse.c)、[电平检测](D:/BW20/ameba-rtos20250911/ameba-rtos/lyy_app/app/APP_Detect.c) 和必要的 GPIO 初始化中，对齐 `GIFT_OUT` 与 `COIN_OUT` 的空闲电平、脉冲极性、脉宽及间隔；两路共用现有 `MODE_CTRL` 检测结果和 `pulse_para`，各自独立计数和排队。新类型仅按脉冲启动，不进入持续电平模式；初始化时先确定空闲电平再使能货道 2 输出，避免上电误触发。

## 验收
- 用你提供的货道、套餐、取码、启动报文回放：确认先收货道再请套餐；测试两种 `positionX` 类型、每路 6 个上限、空货道、同 ID 不同货道、串口来回切换及取码数字精度。
- 验证 `position_x: 2, number: 10` 仅 `PB_21` 输出 10 个脉冲；货道 1 仅 `PA_8` 输出；切换屏幕不影响已发出的订单，两路同时启动互不干扰，非法参数不输出。回归原单路和串口扭蛋机流程。
- 按你的 WSL 流程执行 `source ~/venu/bin/activate` → `cd amebadplus_gcc_project/` → `./build.py`。编译、模拟与台架验证分开报告；上板接机台前需确认原 `GIFT_OUT` 端子的电路支持输出，再测实际极性和波形。