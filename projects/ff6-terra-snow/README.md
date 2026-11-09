# FF6 · 蒂娜雪中行军

RetroDash 歌曲工程，使用用户提供的《Opening Theme (part 3)》SPC，115 秒。主题参考 SFC《Final Fantasy VI》开场的纳尔谢雪中行军。蒂娜驾驶青铜色魔导机甲；敌人为紫灰帝国机甲与步兵，按手动音符交替出现。美术是新制作的像素资源，并非从原游戏提取。

## 游玩与音源

- 78 个手动目标，7 个过密旋律音自动补奏；最小时值 1/8。同一音高固定对应实体键。
- 约 11.6 秒前的引子自动播放；主体选择 voice 0 / SRCN 0x20；84 秒后转入双声部铜管结束句，选择较低的 voice 4 / SRCN 0x23，另一声部自动演奏。
- 按键映射 G3–F5；原始 S-DSP 音高和演奏细节保留，映射不改变音频。
- NEXT 最亮常亮，THEN 较暗并按拍律动；伴奏没有按键灯。KNOB1 调整显示速度 50–200%，不改变歌曲或判定速度。
- 原始 SPC 采样、包络、音高变化、回声通过 fast S-DSP 重放；伴奏默认 30%，手动声部 140%。这不是 MIDI 音色替代。

## 美术与资源

`manifest.json` 是入口；`chart/extraction.json` 记录这首歌的转谱选择和音高校准。

- `art/source-atlas.png`：内置 image_gen 生成的选定源图集。
- `art/terra.0.png` 至 `.5.png`：112×98 原生画布，驾驶舱和脚底基准固定，4 个行走姿态和 2 个射击姿态；动画通过帧序列完成行走、攻击、受击、持续和通关。
- `art/mech.*.png`：88×80 敌方机甲；`art/soldier.*.png`：62×52 步兵；各 3 个姿态。
- `art/far.png`：240×186 不透明远山、纳尔谢与雪原，填满舞台；`village.png`、`foreground.png`、`ground.png` 为不同速度的近景。
- `scene/scene.json`：双敌人列表、2600 ms 默认预读、多层卷轴与 `weather: blizzard`。风雪由低开销动态像素粒子绘制。
- `art/art.json`：图像、动画帧、固定支点、动画序列、角色动作引用；封面不改变通用 UI。

## 从源码构建

使用 RetroDash 0.2.0-beta 或更新的源码。在仓库根目录执行：

```sh
python3 tools/gen_spc_firmware.py projects/ff6-terra-snow
DINGHY_DEFS="-DRD_SPC -DRD_SPC_FAST" JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/sdk ./build.sh --release 0.1.1-beta
```

资源包输出 `build/spc-device/ff6-terra-snow.cqp`；固件输出 `build/retrodash-0.1.1-beta.fwsc`。默认固件同时内置 Peaceful Days 和本曲，实体 SELECT 在选曲页面切换。歌曲工程 ZIP 和 `.cqp` 不能直接通过 USB 导入。上面命令用于单独构建本曲；构建双曲目时执行 `python3 tools/gen_spc_firmware.py projects/peaceful-days-spc projects/ff6-terra-snow`，并使用默认的 `RD_COMPACT` 编译标志。

重新提取原始 SPC：

```sh
python3 tools/build_spc_reference.py projects/ff6-terra-snow/music/source.spc \
  --emulator-dir projects/peaceful-days-spc/vendor/snes_spc \
  --output projects/ff6-terra-snow --work build/ff6-reference \
  --config projects/ff6-terra-snow/chart/extraction.json
```

重新整理美术：`python3 tools/prepare_ff6_art.py`。工具使用内置源图集，不需要再调用图像生成服务。生成提示词见 `art/generation-prompt.txt`。

## 验证范围

- 准确版离线 S-DSP 事件重放与完整 SPC 模拟，补偿接口开头的 4 帧后，3,679,996 个立体声采样帧完全一致。
- 当前 fast S-DSP 游戏路径的整曲模拟：78 次手动命中、78 次自动触发抑制、7 次补奏，满连；暂停/重开、NEXT/THEN 和 KNOB1 检查通过。
- 双敌人、场景数据边界、动态风雪和通用渲染器截图已验证；ASan/UBSan 用于自有适配及游戏代码。
- 单曲与双曲目固件均已编译通过，应用低于槽位 581,564 字节；双曲目加载/切换/重开和独立成绩标识已模拟验证。本曲及双曲目版本尚未刷入真机，不将模拟截图或测试视为真机表现，也未测本曲实际帧率。

来源与权利见 `licenses/credits.txt`；本曲的音乐和角色不随代码的 GPL 重新授权。
