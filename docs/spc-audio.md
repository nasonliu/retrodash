# RetroDash · Peaceful Days SPC 音源工程原型

这是离线验证通过的源工程，当前 FM-1 的 CQP 加载器尚不支持本格式，不能把它作为现有 .cqp 刷入。

## 已完成

- 原始 SPC 原样保存，SHA-256：540cc9ee952c30578818c86f6719c081436b478bb4849137a685885b0fc65f1d。
- 以 SPC 实际演奏为准，保留 8 个硬件声部、原始 BRR、包络、音高变化和原始回声/FIR 设置。
- 实际使用 SRCN 0x20、0x21、0x22、0x23 共 4 个采样，共 14616 字节 BRR。
- 569801 次原始 DSP 写入精简为 11669 次，精简后仍通过整段 PCM 精确比较。时间差编码事件流 47492 字节。
- 1434 次声部触发。游玩主旋律为 voice 0 / SRCN 0x22，从约 8.25 秒开始；开头竖琴自动播放。
- 主旋律 202 个音符，140 个手动目标，62 个密集音自动补奏。固定音高对应固定按键，提示移低两个八度，声音保持原音高。最小手动间隔 0.62 秒。
- 已带入现有 Chrono 主题的原生像素角色、敌人、场景、动画和封面资源，通用 UI 仍由固件提供。

## 验证结果及边界

151 秒完整 SPC 模拟与“只有 S-DSP、没有 SPC700 CPU”的精简事件重放，在补偿 SNES_SPC 接口前置的 4 个立体声采样帧后，比较 4831996 个立体声采样帧，PCM 逐字节一致。这是两个软件重放路径的比较，并非 SFC 实机模拟输出录音，也不是 FM-1 真机声音测试。

SPC 标签为 Chrono Trigger Pre-Release / Yasunori Mitsuda，标记时长 151 秒、淡出 10000 ms。reference.wav 按 151 秒输出，未施加标签淡出、音量调节或后级模拟滤波；SPC 自身会循环，151 秒不是从自然停止检测得到的曲终。

音符名以旧 MIDI 中对应主旋律校准；原始 DSP pitch 值始终保留并用于声音，其他声部不强行转换成 GM 音色或 MIDI 音符。仍需听感核对主旋律键名。

## 工程结构

- manifest.json：工程入口，明确标记固件尚不支持。
- music/source.spc：原始参考。
- audio/aram.bin、dsp-registers.bin：64 KiB 初始音频 RAM 与 128 字节寄存器。
- audio/events.sde：时间差事件流。头 RDSD、版本 1、3 字节保留位，然后 little-endian uint32 clockHz、uint32 eventCount。每条为 ULEB128 deltaClock、uint8 register、uint8 value。
- audio/events-clock.bin：验证用固定 6 字节事件，每条 little-endian uint32 clock、uint8 register、uint8 value。
- audio/voices.json：8 个硬件声部的触发快照和 gate 结束时间，释放尾音由 DSP 包络决定。
- audio/samples/：原始 BRR 与独立解码 WAV；单个 WAV 仅供检查，不包含实际变调/包络/回声。
- chart/chart.json：手动目标、自动补奏及按键映射；伴奏不亮灯，NEXT/THEN 沿用既有规则。
- art/、scene/：角色、敌人、场景资源及引用关系。
- verification/reference.wav、report.json：整段参考和精确比较结果。
- tools/、vendor/snes_spc/：可复现工具与固定版本第三方源码，保留其 LGPL 许可。

## 复现

需要 Python 3、C++ 编译器，无额外 Python 包。工程目录执行：

```
python3 tools/build_spc_reference.py music/source.spc --emulator-dir vendor/snes_spc --output ../rebuilt-audio-project --work ../spc-build
```

命令重新生成音频、谱面和验证数据；主题 art/scene 保持独立资源，可从此工程复制到重建目录。生成器当前针对此文件的文本 ID666 时长和主旋律乐器校准，不宣称支持任意 SPC 自动转谱。

## FM-1 接入方案

推荐离线运行 SPC700，固件只重放 S-DSP 事件。这首歌已证明不需要运行 SPC700，也不需要保留旧 MIDI 的伴奏音源映射。完整 SPC 模拟可作为后续兼容方案，当前优先控制 CPU 占用。

当前游戏固件 .pool 已占 329296 / 344064 字节，不能直接增加完整 64 KiB ARAM。需要替换旧合成引擎并复用其延迟/混响内存；Flash 也需移除被替代的合成表和旧曲目数据。事件加 BRR 约 60.7 KiB，但原型还携带初始 ARAM、检查用 WAV 和重复验证数据，不等于最终固件的占用。

接入时还需完成：S-DSP 定点后端、32 kHz → 44.1 kHz 输出衔接、CQP 新格式的边界校验、命中时触发主旋律和自动补奏调度、暂停/重开状态重置、NEXT/THEN 灯提示与真机负载测试。原曲比对模式保留原音量；游玩模式另做主旋律/伴奏平衡，平衡调整后的声音不再要求与原始混音逐字节一致。需要防止主旋律的自动 KON 与按键 KON 重复触发，且保留自动补奏的音符。

还没有验证 FM-1 同时运行该音源与画面时的帧率，不能据此承诺已达到 60 FPS。

技术参考：
- https://github.com/blarggs-audio-libraries/snes_spc
- https://github.com/blarggs-audio-libraries/snes_spc/blob/main/snes_spc.txt
- https://github.com/blarggs-audio-libraries/snes_spc/blob/main/snes_spc/SPC_DSP.h
