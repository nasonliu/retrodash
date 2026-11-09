# ChipQuest：完整歌曲工程示范

本版把选定的第2种复古像素方向接入了原生FM-1固件。原创短曲SLOW STREET的工程包含MIDI、主旋律/根音配置、角色PNG、敌人PNG、城市背景、地面、封面、帧、动画与主题色。
当前只暴露这一首已加载的示范工程。旧上游曲库暂留在源码供引擎回归使用，不通过产品菜单展示。

## 操作

PLAY进入选曲，再按PLAY开始。SEQ在选曲页切换自动演奏，游玩中也可切换；AUTO状态显示在画面。PLAY暂停，HOME返回。
手动模式只提示下一枚主旋律音符，并只点亮对应的一个实体琴键；伴奏不触发琴键提示，自动演奏模式也不亮琴键灯。下方音名和键盘定位只显示下一个目标，不显示多个待按键。
C4固定对应实体C4，黑键同样保留；主旋律和弦默认取最低音，其他轨自动播放。运行包中有32枚主旋律音符及130枚伴奏音符。

## 文件

- projects/pixel-street/：完整可编辑源工程；manifest、chart、art和scene通过ID引用。PNG与MIDI均在工程内。
- build/demo.cqproj：上述源工程的ZIP，可以独立分享。
- build/demo.cqp：经过校验的设备运行包，CQP2.0，包含音乐和美术。
- firmware/gen/cq_demo.h：同一运行包的只读嵌入形式。固件运行时解析该包，音乐与美术都来自包内；不读取独立PNG或JSON。
- build/vavehero.fwsc：编译出的FM-1可安装固件包。沿用上游包身份FM-1_950，不能仅靠该标识区分不同构建。

## 构建一个工程

需要Python 3.9+、mido、Pillow、Node.js，以及JieLi工具链/SDK（参见docs/DINGHY_README.md）。

```sh
python3 -m pip install mido Pillow
python3 tools/build_project.py projects/pixel-street
# 同样可以输入build/demo.cqproj
python3 tools/build_project.py build/demo.cqproj --firmware
```

第一条生成源工程包、运行包和固件用的资源头文件。第二条额外调用现有固件构建器，需要JIELI_TOOLCHAIN、AC79_SDK和可执行该Linux x86_64工具链的环境。macOS沿用上游Docker支持；本次开发使用Lima Linux+Rosetta构建。
修改歌曲资源时编辑工程文件，不修改界面C代码。换另一份工程重新构建后，播放器从该工程载入音乐及美术。

## 主机模拟和外部加载

在Linux上：

```sh
sh tests/run_host.sh tests/chipquest/project-demo.txt
CQ_PROJECT="$PWD/build/demo.cqp" build/host/vavehero build/external tests/chipquest/project-demo.txt
```

CQ_PROJECT让同一个主机程序读取外部.cqp；未设置时读取固件嵌入包。示范固件在设备上直接从Flash读取嵌入包，因此不需要额外图片解码缓存。
本版没有实现通过USB在线上传/切换.cqp。设备更换歌曲工程目前仍需重新构建并刷写。主机外部加载已验证；尚未刷入本次新固件进行真机测试。

## 当前资源格式与限制

源工程格式为v2，包含PNG/MIDI/JSON/许可信息。运行包使用有边界检查和CRC32的CQP2头、音乐表、RGB565图片、裁切帧、动画时长和动作绑定。
图片最长边240像素，运行包当前预算180KiB；本示范为110900字节。透明像素在运行包中使用保留色键，PNG中半透明像素会以alpha128阈值转换。
第一版支持：一个角色的run/attack/miss/hold/clear动作，一种默认敌人move动作，一个240x128静态背景和一条随音乐滚动的地面；选曲封面与主题色同样来自工程。更换这些内容无需修改界面代码。
场景事件、按单个音符切换敌人皮肤、复杂多层滚动及任意脚本尚未实现；编译器会明确拒绝未支持的事件配置。它不是前一份设计说明全部功能的完成版。
音符时间量化到1/96拍；最多400枚主旋律音符、五条旋律性伴奏轨加鼓轨；不支持曲中变速。主旋律范围F3—G5，超出范围需先移调。伴奏使用内置合成器，不还原原MIDI音源/程序/控制器。

## 验证结果

- 16项Python测试通过：实际轨道选择、根音覆盖、固定琴键、黑键、工程ZIP、缺图、重复ID、帧越界、资源引用顺序等。
- CQP运行时验证通过：音乐指针、资源引用、CRC、截断、越界表与裁切绘制。
- 音符自动松键回归通过；修复了上游短音在离开判定队列后未及时释放的问题。
- 主机自动和手动场景通过AddressSanitizer/UndefinedBehaviorSanitizer。
- 嵌入包和外部.cqp得到逐字节相同的音频和指定时刻画面。
- 灯状态记录：自动81帧最多0键，脚本按键1201帧最多1键。
- JieLi固件构建与RAM/Flash代码布局检查通过。真机帧率、延迟、显示、音频和恢复尚未验证。

## 许可与来源

固件基于VaveHero/Dinghy，GPL-3.0，原说明保存在README.upstream.md及LICENSING.md。示范音乐为本次原创短曲，图片由ImageGen为本项目生成并整理为原生像素资源。详细说明在工程licenses/credits.txt。本源码、源工程、运行包和构建报告一并提供；演示视频为真实固件主机模拟输出，不是概念图。

## 本次视觉/时序修正

示范曲改为原创SLOW STREET，72 BPM，基本每拍一枚主旋律音符。approachMs=1800以毫秒定义敌人进场提前量；多个敌人沿同一路线提前可见，但只为最近目标显示音名和单键灯，不恢复THEN第二键提示。

人物共用0.125缩放、固定帽沿登记位置和(30,43)脚底锚点，四帧奔跑、两帧攻击；所有帧有效高度只相差1像素。武器外伸只影响帧宽，不再缩小整个人物。原始图集及处理脚本随源码提供。

底部按所选设计稿恢复了阶梯箭头边框、粗像素字体、音名键盘和当前键三角指示。使用Pixelify Sans与Press Start 2P（SIL OFL），许可文件位于assets。实际界面按240×240像素实现，不以概念图冒充运行画面。

演示为真实主机固件程序输出的30fps视频，按键由可重复脚本模拟；它不是人类玩家或真机录像。尚未将本版刷入FM-1。
