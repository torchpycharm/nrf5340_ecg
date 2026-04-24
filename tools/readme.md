下面按步骤用 BLE 模式跑通整套链路（PC 端为中心设备，nRF5340 为外围）：
准备工作
- 确认 Python 环境已安装依赖：`pip install bleak pandas scipy pyserial`。
- nRF5340 端：作为 Peripheral，广播并在一个具备 Notify 权限的特征上周期性发送样本。每个 Notify 至少包含 2 字节，小端有符号 int16，代表单个采样点；发送频率建议与 PC 端 `--hz` 相近（默认 300 Hz）。
- 获取外围的 MAC/UUID（Windows Bleak 用蓝牙 MAC，如 `AA:BB:CC:DD:EE:FF`），以及特征 UUID（例如 `0000fff3-0000-1000-8000-00805f9b34fb`）。

启动步骤（两终端）  
1) 终端 A：启动接收 + 网页  
   ```bash
   python test.py --host 0.0.0.0 --port 8000 --hz 300 \
     --ble-address <设备MAC或UUID> --ble-char <特征UUID>
   ```
   看到“Serving index.html and SSE ...”即成功。  
2) 浏览器访问 `http://127.0.0.1:8000/`（或本机 IP:8000），页面波形实时绘制。未收到数据时显示 0。  
3) 终端 B：启动发送端

关键注意事项
- 数据格式匹配：当前 `_on_notify` 仅解析通知的前 2 字节为 int16；请确保每次 Notify 就一个样本。如果一包多样本，需要改代码拆包。
- 采样率匹配：设备发送频率最好与 `--hz` 接近，避免前端看到补零或队列积压。
- 断线重连：连接掉线会自动 2 秒重连，无需手动重启。
- 角色正确：PC 是 Central；nRF5340 必须做 Peripheral 并开启 Notify。  
 
测试数据格式和期望的BLE格式一样
每次读取 2 字节，小端、有符号 int16 作为一个采样点（然后转成 float 放入队列，SSE 推送 {"t":..., "v":...}）。
BLE 这段代码总体是“PC 做中心（Central），订阅 nRF5340 外围（Peripheral）某个特征的 Notify，然后把收到的字节转成 int16 写入队列，再按 `--hz` 速率推到 SSE”。核心流程是对的，能直接接收 nRF5340 网络核用 BLE 发送的数据。关键点与期望数据格式如下：
- 数据解析：回调只取通知的前 2 个字节，按 **小端、有符号 16 位整数** 解析为 `value = int.from_bytes(data[:2], "little", signed=True)`，然后入队 `{t: time.time(), v: float(value)}`。
  - 如果你的 nRF5340 每个 Notify 只发一个采样点（2 字节），这正好匹配。
  - 如果一次 Notify 发多点（例如 10 个 int16 打包 20 字节），现有代码只会用前两个字节，其余会被丢掉，需要改成循环拆包。
- 速率与时序：SSE 推送频率由 PC 端参数 `--hz` 决定（默认 300 Hz）。设备若比 300 Hz 慢，队列为空时前端会收到填充的 0；如果设备比 300 Hz 快，会排队（但前端每帧只取一次 `get_nowait()`，可能丢样），最好让设备与 `--hz` 匹配。
- 断线重连：BLE 断开会捕获异常、等待 `reconnect_delay` 秒（默认 2s）再连。
- 角色正确性：PC 端是中心（Central）；nRF5340 必须做外围（Peripheral）并开启 Notify 的特征。Windows + Bleak 支持中心角色，没问题。

期望收到的数据格式（与 nRF5340 端需要匹配）：
- 每个 Notify 载荷：至少 2 字节，小端、有符号 16 位，表示单个采样值。
- 采样频率：约 300 Hz（或与你启动参数 `--hz` 一致），才能让前端窗口显示实时波形且不填 0。
- 特征属性：具备 Notify 权限即可（不要求 Write）；无需特定服务 UUID，但你需在 `--ble-char` 传入正确特征 UUID。


