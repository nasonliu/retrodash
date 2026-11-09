# 创建 RetroDash 歌曲工程

每首歌用一个工程目录保存音乐、谱面、角色、敌人、背景、动画和封面。界面菜单、判定文字和输入提示由通用 UI 提供。0.2.0-beta 默认同时内置 Peaceful Days 和 FF6 蒂娜主题，实体 SELECT 在选曲页切换。更换内置曲目集合仍需要重新编译并刷入，尚无 USB 曲包导入。

## 先选择音源路线

| 路线 | 音乐来源与音源 | 当前适用范围 | 编译工具 |
| --- | --- | --- | --- |
| MIDI 工程 v2 | MIDI 音符，使用固件合成音色 | 固定速度、音域和音轨数量符合限制的 MIDI | `tools/build_project.py` |
| SPC 工程 v1 | SPC 原采样和 S-DSP 事件，保留原曲包络/音高变化/回声 | Peaceful Days 已真机验证，FF6 蒂娜主题已完成离线/模拟验证；任意 SPC 的自动声部识别/转谱尚未实现 | `tools/gen_spc_firmware.py` |

不要把 MIDI 近似音色与 SPC 原采样重放当成同一种音源。SPC 工程的 `.cqp` 只包含图像和游玩谱面；音频还在另一份生成头文件里，最终 `.fwsc` 才包含完整可刷内容。

## 工程结构

```text
projects/my-song/
  manifest.json          工程入口、名称及资源引用
  music/song.mid         MIDI 路线
  music/source.spc       SPC 路线
  chart/chart.json       主旋律、合并规则、按键映射
  art/art.json           images / frames / clips / actors
  art/*.png              原生像素角色、敌人、背景和封面
  scene/scene.json       场景分层、角色、敌人和进入时间
  licenses/credits.txt   音乐和美术来源、作者、许可
  audio/                 SPC 路线：ARAM、DSP 寄存器、事件、BRR、声部快照
```

JSON 中的资源路径都以当前工程目录为根。资源 ID 可改，但所有引用要一致。请使用自己的音乐和美术，或者确认相应使用许可；固件代码的 GPL 不会自动覆盖第三方歌曲和角色。

## A. 新建通用 MIDI 工程

### 1. 复制模板并换音乐

```sh
cp -R projects/peaceful-days projects/my-song
cp /path/to/your-song.mid projects/my-song/music/song.mid
```

修改 `manifest.json`：`id` 用唯一的 1–48 位 ASCII 文件名字符，例如 `retrodash.my-song`；`title` 用 1–24 位 ASCII 字母、数字、空格或 `_.-`，例如 `MY SONG`。`music` 保持 `music/song.mid`，其余资源引用可先保留。

### 2. 找出主旋律物理音轨

下面脚本只检查 MIDI，不改变文件。索引从 0 开始：

```sh
python3 - <<'CHECK'
from mido import MidiFile
m = MidiFile('projects/my-song/music/song.mid')
for i, track in enumerate(m.tracks):
    notes = [n.note for n in track if n.type == 'note_on' and n.velocity]
    print(i, track.name, len(notes), (min(notes), max(notes)) if notes else '-')
CHECK
```

在 `chart/chart.json` 中设置：

```json
{
  "melodyTrack": 3,
  "chordSelection": {"default": "lowest", "overrides": []},
  "keyMapping": "fm1-fixed-midi-53-79",
  "melodyKeyOctaveShift": -2,
  "minimumTapBeats": 0.5
}
```

- `melodyTrack` 是文件中的物理轨索引，不是 MIDI channel 编号。
- `minimumTapBeats: 0.5` 表示八分音符门槛。过密部分改为自动补奏，不删除原曲声音。
- `melodyKeyOctaveShift` 只移动提示和实体键映射，原音高保留。选择一个整数，使所有游玩音符都落入 F3–G5，即 MIDI 53–79。
- 同时发生的和弦默认选择最低音。需要修改时，`overrides` 写成 `[{"tick": 1920, "pitch": 60}]`，使用原 MIDI 的 tick 和原始音高。
- 复制模板后，请删除不属于新文件的旧 `backingParts` 映射。五条以内的旋律伴奏轨可让工具自动分配；更多轨道必须显式将每条伴奏轨路由到 part 1–5。同类乐器可合用 part，所有伴奏音符仍保留。
- 旧 `patches` 是模板的合成音色选择，可先删掉让它使用默认值。

MIDI 限制：不支持异步 Type 2/SMPTE 时间；不支持中途变速；手动目标最多 400 个；时值量化到 1/96 拍；不支持的 GM 鼓音符会明确报错。文件过密时先做八分音符简化，再考虑拆分歌曲。

### 3. 编译并选择 MIDI 音源

需要 Python 的 `mido`、`Pillow`，以及 Node.js（生成基础表）：

```sh
python3 -m pip install mido Pillow
python3 tools/build_project.py projects/my-song
# MIDI 路线明确关闭 SPC 编译标志：
DINGHY_DEFS="" JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/sdk ./build.sh
```

产物是 `build/demo.cqproj`、`build/demo.cqp` 和 `build/retrodash.fwsc`。`.cqproj` 是可编辑源工程 ZIP；`.cqp` 是紧凑运行时图像/谱面；`.fwsc` 是可刷固件。

## B. 创建或修改 SPC 示范工程

### 1. 从现有 SPC 工程复制

```sh
cp -R projects/peaceful-days-spc projects/my-spc-song
```

修改 `manifest.json` 的 `id`/`title`，再编辑工程自己的 `art/`、`scene/`、`chart/`。这个路线的格式为 `retrodash-spc-project`、版本 1；不要把它改成 MIDI v2 的 manifest。

当前编译器会直接读取选中 SPC 工程自己的美术，不依赖另一个 MIDI 工程的图片。

```sh
python3 tools/gen_spc_firmware.py projects/my-spc-song
JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/sdk ./build.sh
```

`build.sh` 默认启用 `RD_SPC` 和 `RD_SPC_FAST`。编译生成：

- `firmware/gen/rd_spc_data.h`：压缩 ARAM、初始寄存器、事件和手动目标。
- `firmware/gen/cq_spc_demo.h`：此工程的图像、场景和谱面。
- `firmware/gen/kf_spc_song.h`：内置歌曲槽。
- `build/spc-device/<工程目录名>.cqp`：内容来自你指定的目录。

### 2. 重新提取当前这首 SPC

```sh
python3 tools/build_spc_reference.py "/path/to/Peaceful Days.spc" \
  --emulator-dir projects/peaceful-days-spc/vendor/snes_spc \
  --output projects/my-spc-song \
  --work build/my-spc-reference
python3 tools/gen_spc_firmware.py projects/my-spc-song
```

提取器会先运行完整 SPC 模拟，再比较“事件重放 + 独立 S-DSP”的整段 PCM。比较失败，或采样内存在播放过程中变化，工具会停止；不要绕过后继续刷入。重新提取会覆盖 `chart/chart.json`，应先保存自己改过的谱面。

### 3. 任意新 SPC 的当前限制

本版不是自动识别旋律的通用 SPC 转谱器。Peaceful Days 的默认配置如下；FF6 使用自己的 `chart/extraction.json`：

- 手动声部是 DSP voice 0，主旋律 SRCN 为 `0x22`；开头竖琴是自动伴奏。
- `build_spc_reference.py` 中的音高参考值、87.27 BPM、八分拍网格和移低两个八度，针对本曲。SRCN 是采样编号，不是 General MIDI 乐器号。
- 音频结束时长取自 SPC 标记并生成到头文件；主旋律声部与 SRCN 由歌曲配置选择，支持分段换部。换曲仍需提供正确的声部选择、BPM、音高校准，再做离线和真机验证。
- 可先看 `audio/voices.json` 的 voice、SRCN、pitch、clock，以及 `audio/samples/*.wav`，确认哪条是真正主旋律；原始 pitch 寄存器必须保留，不能用猜测的 MIDI 音高替代声音。
- `chart/chart.json` 的 `targets` 是手动目标，`automaticMelodyFills` 是合并后自动补奏；`noteIndex` 指向 `audio/voices.json`。改音符时要保持对应关系，避免主旋律被重复触发或消失。

因此，换一首任意歌曲目前推荐先走 MIDI 路线；需要保留 SFC 原采样的曲目，应按上述开发者流程移植，而不是只改 SPC 文件名。

## C. 更换角色、敌人和背景

### 图片与动画

`art/art.json` 分为四层：

1. `images`：图片 ID、PNG 相对路径、编码方式和共享调色板。
2. `frames`：图片中的 `rect: [x,y,w,h]` 以及 `pivot: [x,y]`。
3. `clips`：帧列表、每帧 `durationMs`、是否循环。
4. `actors`：角色动作到 clip 的映射。

主角需要 `run`、`attack`、`miss`、`hold`、`clear`；敌人的 `move` 由 `scene.targetSkins` 引用。可以先保留模板 ID，逐张替换图片，再调整裁切区域。

所有人物和背景按原生像素显示。建议主角身高约 90 px、敌人约 60 px，门洞至少与人物相称。帧裁切可以不同，但要在统一逻辑画布中对齐脚底/身体基准，裁切后相应扣减 pivot；否则动画会跳位置、看似变大变小。

当前图片单张最大 240×240，最多 32 张图、256 个帧、64 个 clip、4096 个动画步骤；每帧时长 1–2000 ms。优先用 RGBA PNG 的真实透明通道。`indexed4` 使用 15 个不透明色，`indexed8` 使用 255 个不透明色，另外保留透明索引。需要颜色一致的主角图集共享 `paletteGroup`。

### 三层卷轴与地面

模板的点对点尺寸及位置：

| 图层 | 图片尺寸 | y / 高度 | scrollRatio |
| --- | --- | --- | --- |
| far | 240×186 | 28 / 186 | 0.10 |
| village | 240×168 | 28 / 168 | 0.35 |
| foreground | 240×16 | 182 / 16 | 0.75 |
| ground | 72×18，可横向平铺 | 196 | 1.00 |

远景必须不透明并覆盖整个战斗背景。远景与中景采用 `indexed8`、同一个 `paletteGroup: "scene"`，以满足运行时的共享调色板检查。前景设置 `foreground: true`；地面基线保持 y=196。通过调整构图中的房屋/树木尺寸建立比例，不靠运行时拉伸图片。

`scene.approachMs` 是 100% 流速时从远处到判定线的基准时间，范围 1000–3000 ms；模板为 2400 ms。KNOB1 在运行时按 50–200% 调整该时间，且不改变音乐播放速度。

## D. 检查、试听、刷入

```sh
# Linux 上的项目/渲染/判定回归：
sh tests/run_retrodash.sh
# 确认编译输出中的 Flash、RAM、pool 均未超限，再安装：
python3 tools/fm1_install.py build/retrodash.fwsc --port Felucca --yes
python3 tools/fm1_install.py --info --port Felucca
```

程序编译会检查空间限制。当前 SPC 示范曲的美术/谱面约 152 KB，音频和主程序另计；运行时包上限 180 KiB，不能按 PNG 文件大小估算最终占用。

新工程至少检查：主旋律是否正确、固定键映射、伴奏平衡、被合并音的自动补奏、NEXT/THEN 灯、人物脚底对齐、不同 KNOB1 流速、暂停/重开、结算，以及超过 **179 秒** 的持续运行。只跑完一首 151 秒的歌不能覆盖硬件计时器回卷。


## FF6 蒂娜主题示例与新配置

完整示例见 [FF6 蒂娜雪中行军](../projects/ff6-terra-snow/README.md)。选择工程后执行：

```sh
python3 tools/gen_spc_firmware.py projects/ff6-terra-snow
```

离线提取工具新增 `--config projects/ff6-terra-snow/chart/extraction.json`，用 `melodyParts` 指定声部、SRCN 和时间范围，`pitchCalibration` 给出音高对应点，`keyOctaveShift` 控制实体琴键映射。时长来自 SPC 标记，并写入固件；手动目标携带自己的声部，不再固定在 voice 0 / SRCN 0x22。FF6 结尾双声部铜管选择低音声部 4，声部 5 自动演奏。新曲仍需要人工辨认主旋律与校准，不能直接套用 FF6 数值。

`scene.targetSkins` 支持最多 16 个敌人动作条目，按音符索引循环分配；`weather: "blizzard"` 开启像素风雪。编译器在原 CQP2 后附加可选 RDX1 场景信息，当前固件读取并验证，旧曲无需改变。通用 UI、主旋律提示和判定保持一致。


## 同时内置两首歌（0.2.0-beta）

```sh
python3 tools/pack_ui.py
python3 tools/gen_spc_firmware.py projects/peaceful-days-spc projects/ff6-terra-snow
JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/sdk ./build.sh --release 0.2.0-beta
```

不提供工程参数时，生成器默认选择上述两首。`build.sh` 默认开启 `RD_SPC / RD_SPC_FAST / RD_COMPACT`。生成器为各曲分别保留谱面、音频、时长、主旋律声部与美术；选曲画面显示 01/02、02/02，转动 SELECT 切换。旧曲的成绩 hash 和设置数据格式未变。

图片与 UI 像素、DSP 事件均无损压缩；ARAM 只保留 S-DSP 读取的采样目录、BRR 和回声区。已对这两首精简快照做完整准确版 PCM 对比，均与原快照一致。新 SPC 的精简快照仍需单独验证。运行时按图像/小事件块解码，不把两首展开后常驻 RAM。标准编译器继续检查应用、BSS 和 pool 容量；不能保证任意数量或长度的曲包都能放下。

这不使用额外用户存储区，不改变设置/成绩的存储位置。新双曲目版本的实际帧率和音频负载尚待真机验证。
